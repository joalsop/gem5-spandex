#include "mem/ruby/system/SpandexSequencer.hh"

#include "arch/x86/ldstflags.hh"
#include "base/logging.hh"
#include "base/str.hh"
#include "cpu/testers/rubytest/RubyTester.hh"
#include "debug/DenovoRegion.hh"
#include "debug/MemoryAccess.hh"
#include "debug/ProtocolTrace.hh"
#include "debug/RubySequencer.hh"
#include "debug/RubyStats.hh"
#include "mem/packet.hh"
#include "mem/protocol/PrefetchBit.hh"
#include "mem/protocol/RubyAccessMode.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "mem/ruby/system/Sequencer.hh"
#include "sim/denovo_region_table.hh"
#include "sim/system.hh"

using namespace std;

SpandexSequencer *
SpandexSequencerParams::create()
{
    return new SpandexSequencer(this);
}

SpandexSequencer::SpandexSequencer(const Params *p)
    : Sequencer(p)
{
}

SpandexSequencer::~SpandexSequencer()
{
}


RequestStatus
SpandexSequencer::makeRequest(PacketPtr pkt)
{
    if (m_outstanding_count >= m_max_outstanding_requests) {
        return RequestStatus_BufferFull;
    }

    /*
     * WHD:
     * Block requests to lines where there are already outstanding requests
     * Simulated with BufferFull status, even if not true.
     * TODO replace with accurate status descripter */
    Addr line_addr = makeLineAddress(pkt->getAddr());
    if ( (m_writeRequestTable.count(line_addr) > 0) ||
            (m_readRequestTable.count(line_addr) > 0)) {
        return RequestStatus_BufferFull;
    }

    RubyRequestType primary_type = RubyRequestType_NULL;

    if (pkt->isLLSC()) {
        //
        // Alpha LL/SC instructions need to be handled carefully by the cache
        // coherence protocol to ensure they follow the proper semantics. In
        // particular, by identifying the operations as atomic, the protocol
        // should understand that migratory sharing optimizations should not
        // be performed (i.e. a load between the LL and SC should not steal
        // away exclusive permission).
        //
        if (pkt->isWrite()) {
            DPRINTF(RubySequencer, "Issuing SC\n");
            primary_type = RubyRequestType_Store_Conditional;
        } else {
            DPRINTF(RubySequencer, "Issuing LL\n");
            assert(pkt->isRead());
            primary_type = RubyRequestType_Load_Linked;
        }
    } else if (pkt->req->isLockedRMW()) {
        //
        // x86 locked instructions are translated to store cache coherence
        // requests because these requests should always be treated as read
        // exclusive operations and should leverage any migratory sharing
        // optimization built into the protocol.
        //
        if (pkt->isWrite()) {
            DPRINTF(RubySequencer, "Issuing Locked RMW Write\n");
            primary_type = RubyRequestType_Locked_RMW_Write;
        } else {
            DPRINTF(RubySequencer, "Issuing Locked RMW Read\n");
            assert(pkt->isRead());
            primary_type = RubyRequestType_Locked_RMW_Read;
        }
    } else {
        //
        // To support SwapReq, we need to check isWrite() first: a SwapReq
        // should always be treated like a write, but since a SwapReq implies
        // both isWrite() and isRead() are true, check isWrite() first here.
        //
        if (pkt->isWrite()) {
            //
            // Note: M5 packets do not differentiate ST from RMW_Write
            //
            primary_type = RubyRequestType_ST;
        } else if (pkt->isRead()) {
            if (pkt->req->isInstFetch()) {
                primary_type = RubyRequestType_IFETCH;
            } else {
                bool storeCheck = false;
                // only X86 need the store check
                if (system->getArch() == Arch::X86ISA) {
                    uint32_t flags = pkt->req->getFlags();
                    storeCheck = flags &
                        (X86ISA::StoreCheck << X86ISA::FlagShift);
                }
                if (storeCheck) {
                    primary_type = RubyRequestType_RMW_Read;
                } else {
                    primary_type = RubyRequestType_LD;
                }
            }
        } else if (pkt->isFlush()) {
          primary_type = RubyRequestType_FLUSH;
        } else {
            panic("Unsupported ruby packet type\n");
        }
    }

    RubyRequestType secondary_type =
        DenovoRegionTable::get_instance().get_request_type(pkt, true);

    DPRINTF(DenovoRegion, "Primary/secondary type 0x%x %s %s\n", pkt->req->getVaddr(), primary_type, secondary_type);

    if (!DenovoRegionTable::get_instance().get_mem_sim_thread(pkt->req->contextId())) {
        /* note: (on memory management) delete gets called by
           Event::release after doSimLoop() executes this event. */
        auto event = new EventFunctionWrapper{
            [this, pkt, primary_type](){
                DataBlock dataBlk;
                auto seqReq = new SequencerRequest{pkt, primary_type, curCycle()};
                this->hitCallback(seqReq, dataBlk, true, MachineType_NUM, false, Cycles{0}, Cycles{0}, Cycles{0});
            },
            "mem_sim_off hitcallback, bypass"
        };
        schedule(event, curTick() + 100);
        return RequestStatus_Issued;
    }

    RequestStatus status = insertRequest(pkt, primary_type);
    if (status != RequestStatus_Ready)
        return status;

    issueRequest(pkt, secondary_type);

    // TODO: issue hardware prefetches here
    return RequestStatus_Issued;
}

void
SpandexSequencer::readCallback(Addr address, DataBlock& data,
                        bool externalHit, const MachineType mach,
                        Cycles initialRequestTime,
                        Cycles forwardRequestTime,
                        Cycles firstResponseTime)
{
    Addr lineAddr = makeLineAddress(address);
    int wordIndex = (address - lineAddr);

    assert(m_readRequestTable.count(makeLineAddress(address)));

    RequestTable::iterator i = m_readRequestTable.find(lineAddr);
    assert(i != m_readRequestTable.end());
    SequencerRequest* request = i->second;

    DPRINTF(RubySequencer, "readCallback [0x%x, line 0x%x] %s\n", address, lineAddr, request->pendingReqMask);

    // Unset pending mask for returned
    request->pendingReqMask.unsetMask(wordIndex, BYTES_PER_WORD);
    //Only trigger actuall code if all requested words have been returned
    if (request->pendingReqMask.isEmpty()) {
        m_readRequestTable.erase(i);
        markRemoved();

        assert((request->m_type == RubyRequestType_LD) ||
               (request->m_type == RubyRequestType_IFETCH));

        hitCallback(request, data, true, mach, externalHit,
                    initialRequestTime, forwardRequestTime, firstResponseTime);
    }
}

void
SpandexSequencer::writeCallback(Addr address, DataBlock& data,
                         const bool externalHit, const MachineType mach,
                         const Cycles initialRequestTime,
                         const Cycles forwardRequestTime,
                         const Cycles firstResponseTime)
{
    Addr lineAddr = makeLineAddress(address);
    int wordIndex = (address - lineAddr);

    assert(m_writeRequestTable.count(lineAddr));

    RequestTable::iterator i = m_writeRequestTable.find(lineAddr);
    assert(i != m_writeRequestTable.end());
    SequencerRequest* request = i->second;

    DPRINTF(RubySequencer, "writeCallback [0x%x, line 0x%x] %s\n", address, lineAddr, request->pendingReqMask);

    // Unset pending mask for returned
    request->pendingReqMask.unsetMask(wordIndex, BYTES_PER_WORD);
    //Only trigger actuall code if all requested words have been returned
    if (request->pendingReqMask.isEmpty()) {
        m_writeRequestTable.erase(i);
        markRemoved();

        assert((request->m_type == RubyRequestType_ST) ||
               (request->m_type == RubyRequestType_ATOMIC) ||
               (request->m_type == RubyRequestType_RMW_Read) ||
               (request->m_type == RubyRequestType_RMW_Write) ||
               (request->m_type == RubyRequestType_Load_Linked) ||
               (request->m_type == RubyRequestType_Store_Conditional) ||
               (request->m_type == RubyRequestType_Locked_RMW_Read) ||
               (request->m_type == RubyRequestType_Locked_RMW_Write) ||
               (request->m_type == RubyRequestType_FLUSH));

        //
        // For Alpha, properly handle LL, SC, and
        // write requests with respect to locked cache blocks.
        //
        // Not valid for Garnet_standalone protocl
        // WHD: made lineAddr for spandex rn, not sure how SC is handled
        // by spandex.  TODO Talk to John
        bool success = true;
        if (!m_runningGarnetStandalone)
            success = handleLlsc(lineAddr, request);

        // Handle SLICC block_on behavior for Locked_RMW accesses. NOTE: the
        // address variable here is assumed to be a line address, so when
        // blocking buffers, must check line addresses.
        if (request->m_type == RubyRequestType_Locked_RMW_Read) {
            // blockOnQueue blocks all first-level cache controller queues
            // waiting on memory accesses for the specified address that go to
            // the specified queue. In this case, a Locked_RMW_Write must go to
            // the mandatory_q before unblocking the first-level controller.
            // This will block standard loads, stores, ifetches, etc.
            m_controller->blockOnQueue(address, m_mandatory_q_ptr);
        } else if (request->m_type == RubyRequestType_Locked_RMW_Write) {
            m_controller->unblock(address);
        }

        hitCallback(request, data, success, mach, externalHit,
                    initialRequestTime, forwardRequestTime, firstResponseTime);
    }
}

void
SpandexSequencer::issueRequest(PacketPtr pkt, RubyRequestType secondary_type)
{
    assert(pkt != NULL);
    ContextID proc_id = pkt->req->hasContextId() ?
        pkt->req->contextId() : InvalidContextID;

    ContextID core_id = coreId();

    // If valid, copy the pc to the ruby request
    Addr pc = 0;
    if (pkt->req->hasPC()) {
        pc = pkt->req->getPC();
    }

    // check if the packet has data as for example prefetch and flush
    // requests do not
    // Access mask and data block added from Sequencer class
    int blockSize = RubySystem::getBlockSizeBytes();
    std::vector<bool> access_mask(blockSize, true);
    DataBlock dataBlock;
    dataBlock.clear();
    std::shared_ptr<RubyRequest> msg =
        std::make_shared<RubyRequest>(clockEdge(), pkt->getAddr(),
                                      pkt->isFlush() ?
                                      nullptr : pkt->getPtr<uint8_t>(),
                                      pkt->getSize(), pc, secondary_type,
                                      RubyAccessMode_Supervisor, pkt,
                                      PrefetchBit_No, proc_id, core_id,
                                      blockSize, access_mask, dataBlock);

    DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s %#x %s\n",
            curTick(), m_version, "Seq", "Begin", "", "",
            printAddress(msg->getPhysicalAddress()),
            RubyRequestType_to_string(secondary_type));

    // The Sequencer currently assesses instruction and data cache hit latency
    // for the top-level caches at the beginning of a memory access.
    // TODO: Eventually, this latency should be moved to represent the actual
    // cache access latency portion of the memory access. This will require
    // changing cache controller protocol files to assess the latency on the
    // access response path.
    Cycles latency(0);  // Initialize to zero to catch misconfigured latency
    if (secondary_type == RubyRequestType_IFETCH)
        latency = m_inst_cache_hit_latency;
    else
        latency = m_data_cache_hit_latency;

    // Send the message to the cache controller
    assert(latency > 0);

    assert(m_mandatory_q_ptr != NULL);
    m_mandatory_q_ptr->enqueue(msg, clockEdge(), cyclesToTicks(latency));
}

// Insert the request on the correct request table.  Return true if
// the entry was already present.
RequestStatus
SpandexSequencer::insertRequest(PacketPtr pkt, RubyRequestType request_type)
{
    assert(m_outstanding_count ==
        (m_writeRequestTable.size() + m_readRequestTable.size()));

    // See if we should schedule a deadlock check
    if (!deadlockCheckEvent.scheduled() &&
        drainState() != DrainState::Draining) {
        schedule(deadlockCheckEvent, clockEdge(m_deadlock_threshold));
    }

    Addr line_addr = makeLineAddress(pkt->getAddr());

    // Check if the line is blocked for a Locked_RMW
    if (m_controller->isBlocked(line_addr) &&
        (request_type != RubyRequestType_Locked_RMW_Write)) {
        // Return that this request's cache line address aliases with
        // a prior request that locked the cache line. The request cannot
        // proceed until the cache line is unlocked by a Locked_RMW_Write
        return RequestStatus_Aliased;
    }

    // Create a default entry, mapping the address to NULL, the cast is
    // there to make gcc 4.4 happy
    RequestTable::value_type default_entry(line_addr,
                                           (SequencerRequest*) NULL);

    // Adding full request mask is added functionality for spandex compared to base class
    // This should be set dynamically if not requesting a full cache line
    std::vector<bool> reqMask(RubySystem::getBlockSizeBytes(), true);

    if ((request_type == RubyRequestType_ST) ||
        (request_type == RubyRequestType_RMW_Read) ||
        (request_type == RubyRequestType_RMW_Write) ||
        (request_type == RubyRequestType_Load_Linked) ||
        (request_type == RubyRequestType_Store_Conditional) ||
        (request_type == RubyRequestType_Locked_RMW_Read) ||
        (request_type == RubyRequestType_Locked_RMW_Write) ||
        (request_type == RubyRequestType_FLUSH)) {

        // Check if there is any outstanding read request for the same
        // cache line.
        if (m_readRequestTable.count(line_addr) > 0) {
            m_store_waiting_on_load++;
            return RequestStatus_Aliased;
        }

        pair<RequestTable::iterator, bool> r =
            m_writeRequestTable.insert(default_entry);
        if (r.second) {
            RequestTable::iterator i = r.first;
            i->second = new SequencerRequest(pkt, request_type, curCycle(), reqMask);
            m_outstanding_count++;
        } else {
          // There is an outstanding write request for the cache line
          m_store_waiting_on_store++;
          return RequestStatus_Aliased;
        }
    } else {
        // Check if there is any outstanding write request for the same
        // cache line.
        if (m_writeRequestTable.count(line_addr) > 0) {
            m_load_waiting_on_store++;
            return RequestStatus_Aliased;
        }

        pair<RequestTable::iterator, bool> r =
            m_readRequestTable.insert(default_entry);

        if (r.second) {
            RequestTable::iterator i = r.first;
            i->second = new SequencerRequest(pkt, request_type, curCycle(), reqMask);
            m_outstanding_count++;
        } else {
            // There is an outstanding read request for the cache line
            m_load_waiting_on_load++;
            return RequestStatus_Aliased;
        }
    }

    m_outstandReqHist.sample(m_outstanding_count);
    assert(m_outstanding_count ==
        (m_writeRequestTable.size() + m_readRequestTable.size()));

    return RequestStatus_Ready;
}
