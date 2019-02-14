#ifndef __DENOVO_HH__
#define __DENOVO_HH__

#include <map>
#include <utility>

#include "gem5/denovo_region_api.h"

#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"

// Singleton class
class DenovoRegionTable {
public:

    static DenovoRegionTable& get_instance() {
        static DenovoRegionTable instance;
        return instance;
    }

    DenovoRegionTable(DenovoRegionTable const&) = delete;

    void operator=(DenovoRegionTable const&) = delete;

    void put_region(Addr begin, size_t length, ContextID context_id,
        mem_policy_t read_policy, mem_policy_t write_policy, mem_policy_t rmw_policy);

    RubyRequestType get_request_type(PacketPtr pkt, bool x86 = false);

    void set_mem_sim_all(bool on) {
        for (const auto& pair : _mem_sim_thread) {
            _mem_sim_thread[pair.first] = on;
        }
        _mem_sim_default = on;
    }
    void set_mem_sim_thread(bool on, ContextID context) {
        _mem_sim_thread[context] = on;
    }
    bool get_mem_sim_thread(ContextID context) {
        const auto& found = _mem_sim_thread.find(context);
        if (found != _mem_sim_thread.end()) {
            return found->second;
        } else {
            return _mem_sim_default;
        }
    }
private:

    DenovoRegionTable() = default;

    // region_map[context_id] has a map from start_address -> (end_address, (r_policy, w_policy, rmw_policy))
    std::map<ContextID, std::map<Addr, std::pair<Addr, std::tuple<mem_policy_t, mem_policy_t, mem_policy_t>>>>
        region_map;

    std::tuple<mem_policy_t, mem_policy_t, mem_policy_t>
    get_region(Addr addr, ContextID context_id);

    std::tuple<mem_policy_t, mem_policy_t, mem_policy_t>
    get_region_helper(Addr addr, ContextID context_id);

    bool _mem_sim_default;
    std::map<ContextID, bool> _mem_sim_thread;

};

#endif // __DENOVO_HH__
