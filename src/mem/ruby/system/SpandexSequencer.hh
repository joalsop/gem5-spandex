#ifndef __MEM_RUBY_SYSTEM_SPANDEXSEQUENCER_HH__
#define __MEM_RUBY_SYSTEM_SPANDEXSEQUENCER_HH__

#include <iostream>
#include <unordered_map>

#include "mem/protocol/MachineType.hh"
#include "mem/protocol/RubyRequestType.hh"
#include "mem/protocol/SequencerRequestType.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/structures/CacheMemory.hh"
#include "mem/ruby/system/RubyPort.hh"
#include "mem/ruby/system/Sequencer.hh"
#include "params/SpandexSequencer.hh"

class SpandexSequencerParams;

class SpandexSequencer : public Sequencer
{
    public:
        typedef SpandexSequencerParams Params;
        SpandexSequencer(const Params*);
        ~SpandexSequencer();
        // Make sure the copy constructor cannot be called
        SpandexSequencer(const SpandexSequencer&)=delete;

        void writeCallback(Addr address,
                           DataBlock& data,
                           const bool externalHit = false,
                           const MachineType mach = MachineType_NUM,
                           const Cycles initialRequestTime = Cycles(0),
                           const Cycles forwardRequestTime = Cycles(0),
                           const Cycles firstResponseTime = Cycles(0));

        void readCallback(Addr address,
                          DataBlock& data,
                          const bool externalHit = false,
                          const MachineType mach = MachineType_NUM,
                          const Cycles initialRequestTime = Cycles(0),
                          const Cycles forwardRequestTime = Cycles(0),
                          const Cycles firstResponseTime = Cycles(0));
        RequestStatus makeRequest(PacketPtr pkt);
        RequestStatus insertRequest(PacketPtr pkt, RubyRequestType request_type);
        void issueRequest(PacketPtr pkt, RubyRequestType type);
};
#endif // __MEM_RUBY_SYSTEM_SPANDEXSEQUENCER_HH__
