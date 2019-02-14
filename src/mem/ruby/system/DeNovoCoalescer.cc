/*
 * Copyright (c) 2013-2015 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * For use for simulation and test purposes only
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Sooraj Puthoor, Tuan Ta
 */

#include "base/logging.hh"
#include "base/str.hh"
#include "config/the_isa.hh"

#if THE_ISA == X86_ISA
#include "arch/x86/insts/microldstop.hh"

#endif // X86_ISA
#include "mem/ruby/system/DeNovoCoalescer.hh"

#include "cpu/testers/rubytest/RubyTester.hh"
#include "debug/GPUCoalescer.hh"
#include "debug/MemoryAccess.hh"
#include "debug/ProtocolTrace.hh"
#include "debug/WHD.hh"
#include "mem/packet.hh"
#include "mem/ruby/common/SubBlock.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/slicc_interface/AbstractController.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"
#include "mem/ruby/structures/CacheMemory.hh"
#include "mem/ruby/system/GPUCoalescer.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "params/DeNovoCoalescer.hh"
#include "sim/denovo_region_table.hh"

using namespace std;

DeNovoCoalescer *
DeNovoCoalescerParams::create()
{
    return new DeNovoCoalescer(this);
}

DeNovoCoalescer::DeNovoCoalescer(const Params *p)
    : GPUCoalescer(p),
      m_cache_inv_pkt(nullptr),
      m_num_pending_invs(0)
{
}

DeNovoCoalescer::~DeNovoCoalescer()
{
}

// Places an uncoalesced packet in uncoalescedTable. If the packet is a
// special type (MemFence, scoping, etc), it is issued immediately.
RequestStatus
DeNovoCoalescer::makeRequest(PacketPtr pkt)
{
    // VIPER only supports following memory request types
    //    MemSyncReq & INV_L1 : TCP cache invalidation
    //    ReadReq             : cache read
    //    WriteReq            : cache write
    //    AtomicOp            : cache atomic
    //
    // VIPER does not expect MemSyncReq & Release since in GCN3, compute unit
    // does not specify an equivalent type of memory request.
    assert((pkt->cmd == MemCmd::MemSyncReq && pkt->req->isInvL1()) ||
            pkt->cmd == MemCmd::ReadReq ||
            pkt->cmd == MemCmd::WriteReq ||
            pkt->isAtomicOp());

    if (pkt->req->isInvL1() && m_cache_inv_pkt) {
        // In VIPER protocol, the coalescer is not able to handle two or
        // more cache invalidation requests at a time. Cache invalidation
        // requests must be serialized to ensure that all stale data in
        // TCP are invalidated correctly. If there's already a pending
        // cache invalidation request, we must retry this request later
        return RequestStatus_Aliased;
    }

    GPUCoalescer::makeRequest(pkt);

    if (pkt->req->isInvL1()) {
        // In VIPER protocol, a compute unit sends a MemSyncReq with INV_L1
        // flag to invalidate TCP. Upon receiving a request of this type,
        // VIPERCoalescer starts a cache walk to invalidate all valid entries
        // in TCP. The request is completed once all entries are invalidated.
        assert(!m_cache_inv_pkt);
        m_cache_inv_pkt = pkt;
        invTCP();
    }

    return RequestStatus_Issued;
}

void
DeNovoCoalescer::issueRequest(CoalescedRequest* crequest)
{
    //DPRINTF(WHD, "DeNovo issueRequest\n");
    PacketPtr pkt = crequest->getFirstPkt();

    int proc_id = -1;
    if (pkt != NULL && pkt->req->hasContextId()) {
        proc_id = pkt->req->contextId();
    }

    // If valid, copy the pc to the ruby request
    Addr pc = 0;
    if (pkt->req->hasPC()) {
        pc = pkt->req->getPC();
    }

    Addr line_addr = makeLineAddress(pkt->getAddr());

    // Creating WriteMask that records written bytes
    // and atomic operations. This enables partial writes
    // and partial reads of those writes
    DataBlock dataBlock;
    dataBlock.clear();
    uint32_t blockSize = RubySystem::getBlockSizeBytes();
    std::vector<bool> accessMask(blockSize,false);
    std::vector< std::pair<int,AtomicOpFunctor*> > atomicOps;
    uint32_t tableSize = crequest->getPackets().size();
    for (int i = 0; i < tableSize; i++) {
        PacketPtr tmpPkt = crequest->getPackets()[i];
        /* tmpOffset/Size replaced to force word granularity, which had been incorrectly assumed
         * TODO adjust system to byte granularity (if needed) */
        //uint32_t tmpOffset = (tmpPkt->getAddr()) - line_addr;
        //uint32_t tmpSize = tmpPkt->getSize();
        uint32_t tmpOffset = ((tmpPkt->getAddr()) - line_addr)/BYTES_PER_WORD;
        uint32_t tmpSize = BYTES_PER_WORD;
        if (tmpPkt->isAtomicOp()) {
            std::pair<int,AtomicOpFunctor *> tmpAtomicOp(tmpOffset,
                                                        tmpPkt->getAtomicOp());
            atomicOps.push_back(tmpAtomicOp);
        } else if (tmpPkt->isWrite()) {
            dataBlock.setData(tmpPkt->getPtr<uint8_t>(),
                              tmpOffset, tmpSize);
        }
        for (int j = 0; j < tmpSize; j++) {
            accessMask[tmpOffset*BYTES_PER_WORD + j] = true;
        }
    }
    std::shared_ptr<RubyRequest> msg;
    if (pkt->isAtomicOp()) {
        msg = std::make_shared<RubyRequest>(clockEdge(), pkt->getAddr(),
                              pkt->getPtr<uint8_t>(),
                              pkt->getSize(), pc, crequest->getRubyType(),
                              RubyAccessMode_Supervisor, pkt,
                              PrefetchBit_No, proc_id, 100,
                              blockSize, accessMask,
                              dataBlock, atomicOps, crequest->getSeqNum());
    } else {
        msg = std::make_shared<RubyRequest>(clockEdge(), pkt->getAddr(),
                              pkt->getPtr<uint8_t>(),
                              pkt->getSize(), pc, crequest->getRubyType(),
                              RubyAccessMode_Supervisor, pkt,
                              PrefetchBit_No, proc_id, 100,
                              blockSize, accessMask,
                              dataBlock, crequest->getSeqNum());
    }

    /* Use accessMask to set pendingReqMask */
    crequest->pendingReqMask.cpyMask(msg->m_writeMask);
    //DPRINTF(WHD, "ReqMask: %s\n",msg->m_writeMask);

//    if (pkt->cmd == MemCmd::WriteReq) {
//        makeWriteCompletePkts(crequest);
//    }

    DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s %s %s\n",
             curTick(), m_version, "Coal", "Begin", "", "",
             printAddress(msg->getPhysicalAddress()),
             RubyRequestType_to_string(crequest->getRubyType()));

    fatal_if(crequest->getRubyType() == RubyRequestType_IFETCH,
             "there should not be any I-Fetch requests in the GPU Coalescer");

    // Send the message to the cache controller
    fatal_if(m_data_cache_hit_latency == 0,
             "should not have a latency of zero");

    if (!deadlockCheckEvent.scheduled()) {
        schedule(deadlockCheckEvent,
                 m_deadlock_threshold * clockPeriod() +
                 curTick());
    }

    assert(m_mandatory_q_ptr);
    m_mandatory_q_ptr->enqueue(msg, clockEdge(), m_data_cache_hit_latency);
}

void
DeNovoCoalescer::invTCPCallback(Addr addr)
{
    assert(m_cache_inv_pkt && m_num_pending_invs > 0);

    m_num_pending_invs--;
    if (m_num_pending_invs == 0) {
        std::vector<PacketPtr> pkt_list { m_cache_inv_pkt };
        completeHitCallback(pkt_list);
        m_cache_inv_pkt = nullptr;
    }
}

/**
  * Invalidate TCP
  */
void
DeNovoCoalescer::invTCP()
{
    int size = m_dataCache_ptr->getNumBlocks();
    DPRINTF(GPUCoalescer,
            "There are %d Invalidations outstanding before Cache Walk\n",
            m_num_pending_invs);
    // Walk the cache
    for (int i = 0; i < size; i++) {
        Addr addr = m_dataCache_ptr->getAddressAtIdx(i);
        // Evict Read-only data
        std::shared_ptr<RubyRequest> msg = std::make_shared<RubyRequest>(
            clockEdge(), addr, (uint8_t*) 0, 0, 0,
            RubyRequestType_REPLACEMENT, RubyAccessMode_Supervisor,
            nullptr);
        msg->m_writeMask.fillMask();
        DPRINTF(GPUCoalescer, "Evicting addr 0x%x\n", addr);
        assert(m_mandatory_q_ptr != NULL);
        m_mandatory_q_ptr->enqueue(msg, clockEdge(), m_data_cache_hit_latency);
        m_num_pending_invs++;
    }
    DPRINTF(GPUCoalescer,
            "There are %d Invalidatons outstanding after Cache Walk\n",
            m_num_pending_invs);
}

void
DeNovoCoalescer::readCallback(Addr address, DataBlock& data)
{
    readCallback(address, MachineType_NULL, data);
}

void
DeNovoCoalescer::readCallback(Addr address,
                        MachineType mach,
                        DataBlock& data)
{
    readCallback(address, mach, data, Cycles(0), Cycles(0), Cycles(0));
}

void
DeNovoCoalescer::readCallback(Addr address,
                        MachineType mach,
                        DataBlock& data,
                        Cycles initialRequestTime,
                        Cycles forwardRequestTime,
                        Cycles firstResponseTime)
{

    readCallback(address, mach, data,
                 initialRequestTime, forwardRequestTime, firstResponseTime,
                 false);
}

void
DeNovoCoalescer::readCallback(Addr address,
                        MachineType mach,
                        DataBlock& data,
                        Cycles initialRequestTime,
                        Cycles forwardRequestTime,
                        Cycles firstResponseTime,
                        bool isRegion)
{

    Addr lineAddr = makeLineAddress(address);
    assert(coalescedTable.count(lineAddr));

    int wordIndex = (address - lineAddr);
    coalescedTable.at(lineAddr).front()->
        pendingReqMask.unsetMask(wordIndex, BYTES_PER_WORD);

    DPRINTF(GPUCoalescer, "readCallback called for [0x%x, line 0x%x] table %d\n",
			address, lineAddr, coalescedTable.size());
    /* Basically only proceed with the standard code if all words have
     * returned from the memory system. */
    if (coalescedTable.at(lineAddr).front()->pendingReqMask.isEmpty()) {
        //DPRINTF(WHD, "Enter readCallback\n");
        auto crequest = coalescedTable.at(lineAddr).front();
        fatal_if(crequest->getRubyType() != RubyRequestType_LD,
                 "readCallback received non-read type response\n");

        // Iterate over the coalesced requests to respond to as many loads as
        // possible until another request type is seen. Models MSHR for TCP.
        //
        // TODO this will change from a while to an if under our new design
        // rn this logic should always not be hit bc we dont do multiple issue
        // and we won't just be able to issue the hit, we'll have to actually issue
        while (crequest->getRubyType() == RubyRequestType_LD) {
            //DPRINTF(WHD, "Looping on creq\n");
            //DPRINTF(WHD, "Got readCallback for seqNum: %lu\n",crequest->getSeqNum());
            hitCallback(crequest, mach, data, true, crequest->getIssueTime(),
                        forwardRequestTime, firstResponseTime, isRegion);

            delete crequest;
            coalescedTable.at(lineAddr).pop_front();
            if (coalescedTable.at(lineAddr).empty()) {
                //DPRINTF(WHD, "Coalesced table should be empty\n");
                break;
            }

            crequest = coalescedTable.at(lineAddr).front();
        }

        if (coalescedTable.at(lineAddr).empty()) {
            coalescedTable.erase(lineAddr);
        } else {
            auto nextRequest = coalescedTable.at(lineAddr).front();
            issueRequest(nextRequest);
        }
    }
}

void
DeNovoCoalescer::writeCallback(Addr address, DataBlock& data)
{
    writeCallback(address, MachineType_NULL, data);
}

void
DeNovoCoalescer::writeCallback(Addr address,
                         MachineType mach,
                         DataBlock& data)
{
    writeCallback(address, mach, data, Cycles(0), Cycles(0), Cycles(0));
}

void
DeNovoCoalescer::writeCallback(Addr address,
                         MachineType mach,
                         DataBlock& data,
                         Cycles initialRequestTime,
                         Cycles forwardRequestTime,
                         Cycles firstResponseTime)
{
    writeCallback(address, mach, data,
                  initialRequestTime, forwardRequestTime, firstResponseTime,
                  false);
}

void
DeNovoCoalescer::writeCallback(Addr address,
                         MachineType mach,
                         DataBlock& data,
                         Cycles initialRequestTime,
                         Cycles forwardRequestTime,
                         Cycles firstResponseTime,
                         bool isRegion)
{
    Addr lineAddr = makeLineAddress(address);

    assert(coalescedTable.count(lineAddr));

    int wordIndex = (address - lineAddr);
    coalescedTable.at(lineAddr).front()->
        pendingReqMask.unsetMask(wordIndex, BYTES_PER_WORD);

    DPRINTF(GPUCoalescer, "writeCallback called for [0x%x, line 0x%x] table %d\n",
			address, lineAddr, coalescedTable.size());
    /* Basically only proceed with the standard code if all words have
     * returned from the memory system. */
    if (coalescedTable.at(lineAddr).front()->pendingReqMask.isEmpty()) {
        //DPRINTF(WHD, "Enter writeCallback\n");
        auto crequest = coalescedTable.at(lineAddr).front();
        //DPRINTF(WHD, "Got writeCallback for seqNum: %lu\n",crequest->getSeqNum());
        hitCallback(crequest, mach, data, true, crequest->getIssueTime(),
                    forwardRequestTime, firstResponseTime, isRegion);
        // remove this crequest in coalescedTable
        delete crequest;
        coalescedTable.at(lineAddr).pop_front();

        if (coalescedTable.at(lineAddr).empty()) {
            //DPRINTF(WHD, "coalescedTable for writes should be empty\n");
            coalescedTable.erase(lineAddr);
        } else {
            //DPRINTF(WHD, "coalescedTable for writes should be empty, but isn't!! ERROR\n");
            auto nextRequest = coalescedTable.at(lineAddr).front();
            issueRequest(nextRequest);
        }
    }
}


bool
DeNovoCoalescer::coalescePacket(PacketPtr pkt)
{
    uint64_t seqNum = pkt->req->getReqInstSeqNum();
    Addr line_addr = makeLineAddress(pkt->getAddr());

    // If the packet has the same line address as a request already in the
    // coalescedTable and has the same sequence number, it can be coalesced.
    if (coalescedTable.count(line_addr)) {
        // Search for a previous coalesced request with the same seqNum.
        auto& creqQueue = coalescedTable.at(line_addr);
        auto citer = std::find_if(creqQueue.begin(), creqQueue.end(),
            [&](CoalescedRequest* c) { return c->getSeqNum() == seqNum; }
        );
        if (citer != creqQueue.end()) {
            (*citer)->insertPacket(pkt);
            return true;
        }
    }

    if (m_outstanding_count < m_max_outstanding_requests) {
        // This is an "aliased" or new request. Create a RubyRequest and
        // append it to the list of "targets" in the coalescing table.
        DPRINTF(GPUCoalescer, "Creating new or aliased request for 0x%X\n",
                line_addr);

        CoalescedRequest *creq = new CoalescedRequest(seqNum);
        creq->insertPacket(pkt);
        creq->setRubyType(getRequestType(pkt));
        creq->setIssueTime(curCycle());

        if (!coalescedTable.count(line_addr)) {
            // If there is no outstanding request for this line address,
            // create a new coalecsed request and issue it immediately.
            auto reqList = std::deque<CoalescedRequest*> { creq };
            coalescedTable.insert(std::make_pair(line_addr, reqList));

            DPRINTF(GPUCoalescer, "Issued req type %s seqNum %d [0x%x, line 0x%x]\n",
                    RubyRequestType_to_string(creq->getRubyType()), seqNum, pkt->getAddr(), line_addr);
            issueRequest(creq);
        } else {
            // Block requests if there are outstanding requests for this line
            // TODO We will eventually implement coalescing here and pop off just
            // one upon request completion, but this is not required for correctness


            // NOTE I had previously allowed Writes to be coalesced because of an
            // error I was running into.  This might have been correct (as it
            // didn't affect the correctness of test code, but I didn't document
            // my reasoning enough to convince myself again so I'm stripping it
            // out until coalescing get's formally revisited
            //
            //if (pkt->cmd == MemCmd::WriteReq) {
            //    DPRINTF(WHD, "Coalescing write req\n");
            //    coalescedTable.at(line_addr).push_back(creq);
            //    DPRINTF(GPUCoalescer, "found address 0x%X with new seqNum %d\n",
            //            line_addr, seqNum);
            //} else {
                return false;
            //}
        }

        // In both cases, requests are added to the coalescing table and will
        // be counted as outstanding requests.
        m_outstanding_count++;

        // We track all issued or to-be-issued Ruby requests associated with
        // write instructions. An instruction may have multiple Ruby
        // requests.
        if (pkt->cmd == MemCmd::WriteReq) {
            DPRINTF(GPUCoalescer, "adding write inst %d at line 0x%x to"
                    " the pending write instruction list\n", seqNum,
                    line_addr);

            RubyPort::SenderState* ss =
                    safe_cast<RubyPort::SenderState*>(pkt->senderState);

            // we need to save this port because it will be used to call
            // back the requesting CU when we receive write
            // complete callbacks for all issued Ruby requests of this
            // instruction.
            RubyPort::MemSlavePort* mem_slave_port = ss->port;

            PendingWriteInst& inst = pendingWriteInsts[seqNum];
            inst.addPendingReq(mem_slave_port, pkt, m_usingRubyTester);
        }

        return true;
    }

    // The maximum number of outstanding requests have been issued.
    return false;
}

RubyRequestType
DeNovoCoalescer::getRequestType(PacketPtr pkt)
{

    // These types are not support or not used in GPU caches.
    assert(!pkt->req->isLLSC());
    assert(!pkt->req->isLockedRMW());
    assert(!pkt->req->isInstFetch());
    assert(!pkt->isFlush());

    return DenovoRegionTable::get_instance().get_request_type(pkt);
}
