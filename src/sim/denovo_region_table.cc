#include <algorithm>

#include "base/trace.hh"
#include "debug/DenovoRegion.hh"
#include "sim/denovo_region_table.hh"
#include "arch/x86/ldstflags.hh"

void DenovoRegionTable::put_region(Addr begin, size_t length, ContextID context_id,
        mem_policy_t read_policy, mem_policy_t write_policy, mem_policy_t rmw_policy) {
    DPRINTF(DenovoRegion,
            "put_region(%p, %p+%ld = %p, %u, %u, %u)\n",
            begin, begin, length, begin + length, read_policy, write_policy, rmw_policy);

    // O(log(len(contexts)) + log(len(regions)))
    region_map[context_id][begin] = {begin + length, std::make_tuple(write_policy, read_policy, rmw_policy)};
}

std::tuple<mem_policy_t, mem_policy_t, mem_policy_t>
DenovoRegionTable::get_region_helper(Addr addr, ContextID context_id) {
    using Elem = std::pair<Addr, std::pair<Addr, std::tuple<mem_policy_t, mem_policy_t, mem_policy_t>>>;

    // region_map is sorted by start_address
    // so I need to find the largest start_address less than addr

    // O(log(len(regions)))
    auto it = std::lower_bound(
        // It has to be reversed because
        // std::lower_bound returns the first elem not less than the target
        // but I want the last elem not greater than target
        region_map[context_id].crbegin(),
        region_map[context_id].crend(),

        // This is pair has the start_address of what I am searching for
        std::make_pair(addr,
            std::make_pair(addr,
                std::make_tuple(MEMPOLICY_DEFAULT, MEMPOLICY_DEFAULT, MEMPOLICY_DEFAULT))),

        // only compare them by the begin address (first in the pair)
        // < is switched with > because the iterator is reversed
        [](const Elem& i1, const Elem& i2) { return i1.first > i2.first; }
    );

    if (it != region_map[context_id].crend() && // range found AND
        addr < it->second.first  // addr is less than the end_addr, so addr is in range
        ) {
        return it->second.second;
    } else {
        return std::make_tuple(MEMPOLICY_DEFAULT, MEMPOLICY_DEFAULT, MEMPOLICY_DEFAULT);
    }
}

std::tuple<mem_policy_t, mem_policy_t, mem_policy_t>
DenovoRegionTable::get_region(Addr addr, ContextID context_id) {
    mem_policy_t read_policy, write_policy, rmw_policy;
    std::tie(read_policy, write_policy, rmw_policy) =
        get_region_helper(addr, context_id);

    // Make ALL_CONTEXTS apply when there is no specializaiton for this context_id

    mem_policy_t default_read_policy, default_write_policy, default_rmw_policy;
    std::tie(default_read_policy, default_write_policy, default_rmw_policy) =
        get_region_helper(addr, ALL_CONTEXTS);

    if (read_policy == MEMPOLICY_DEFAULT) {
        read_policy = default_read_policy;
    }
    if (write_policy == MEMPOLICY_DEFAULT) {
        write_policy = default_write_policy;
    }
    if (rmw_policy == MEMPOLICY_DEFAULT) {
        rmw_policy = default_rmw_policy;
    }

    return std::tie(read_policy, write_policy, rmw_policy);
}

// One wrinkle with using thread_context for CPU and workgroup for GPU:
// They are not guaranteed to be unique.
// So I will force the highest bit of thread_context to be 0, and of workgroup_id to be 1.
// Unless you have more than 12 7threads or workgroups and they are assigned sequentially, this won't introduce new conflicts.

RubyRequestType
DenovoRegionTable::get_request_type(PacketPtr pkt, bool x86) {
    mem_policy_t read_policy, write_policy, rmw_policy;
    std::tie(read_policy, write_policy, rmw_policy) =
        get_region(pkt->req->getVaddr(), pkt->req->contextId());

    // this code is an unholy frankenstein of what was once SpandexSequencer::makeRequest and DeNovoCoalescer::getRequestType.
    RubyRequestType ret;
    // initial if (false) makes the other cases identical
    if (false) {
    } else if (pkt->req->isAtomicNoReturn() || pkt->req->isAtomicReturn() || pkt->isLLSC()) {
        ret = RubyRequestType_ATOMIC;
    } else if (pkt->isFlush()) {
        ret = RubyRequestType_FLUSH;
    } else if (pkt->req->isLockedRMW()) {
        ret = RubyRequestType_ST;
    } else if (pkt->isRead()) {
        if (false) {
        } else if (pkt->req->isInstFetch()) {
            ret = RubyRequestType_IFETCH;
        } else {
            bool storeCheck = false;
            // only X86 need the store check
            if (x86) {
                uint32_t flags = pkt->req->getFlags();
                storeCheck = flags &
                    (X86ISA::StoreCheck << X86ISA::FlagShift);
            }

            if (storeCheck) {
                ret = RubyRequestType_ST;
            } else {
                if (false) {
                } else if (read_policy == MEMPOLICY_LINE_GRANULARITY) {
                    ret = RubyRequestType_LD_LG;
                } else if (read_policy == MEMPOLICY_LINE_GRANULARITY_OWNED) {
                    ret = RubyRequestType_LD_LGO;
                } else if (read_policy == MEMPOLICY_SHARED_LINE_GRANULARITY_OWNED) {
                    ret = RubyRequestType_LD_SLGO;
                } else if (read_policy == MEMPOLICY_DEFAULT) {
                    ret = RubyRequestType_LD;
                } else {
                    panic("mem_policy_t %u is not supported for reads.\n", read_policy);
                }
            }
        }
    } else if (pkt->isWrite()) {
        if (false) {
        } else if (write_policy == MEMPOLICY_WRITETHROUGH) {
            ret = RubyRequestType_ST_WT;
        } else if (write_policy == MEMPOLICY_LINE_GRANULARITY_OWNED) {
            ret = RubyRequestType_ST_LGO;
        } else if (write_policy == MEMPOLICY_DEFAULT) {
            ret = RubyRequestType_ST;
        } else {
            panic("mem_policy_t %u is not supported for writes.\n", write_policy);
        }
    } else {
        panic("Unsupported ruby packet type\n");
    }

    DPRINTF(DenovoRegion,
            "get_request_type(%p, %u) = %u\n",
            pkt->req->getVaddr(), pkt->req->contextId(), ret);

    return ret;
}
