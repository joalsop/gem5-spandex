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

#ifndef __MEM_RUBY_SYSTEM_DeNovoCOALESCER_HH__
#define __MEM_RUBY_SYSTEM_DeNovoCOALESCER_HH__

#include <iostream>

#include "mem/protocol/PrefetchBit.hh"
#include "mem/protocol/RubyAccessMode.hh"
#include "mem/protocol/RubyRequestType.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/system/GPUCoalescer.hh"
#include "mem/ruby/system/RubyPort.hh"

class DataBlock;
class CacheMsg;
class MachineID;
class CacheMemory;

class DeNovoCoalescerParams;

class DeNovoCoalescer : public GPUCoalescer
{
  public:
    typedef DeNovoCoalescerParams Params;
    DeNovoCoalescer(const Params *);
    ~DeNovoCoalescer();
    void invTCPCallback(Addr address);
    RequestStatus makeRequest(PacketPtr pkt);
    void issueRequest(CoalescedRequest* crequest);
    void writeCallback(Addr address, DataBlock& data);

    void writeCallback(Addr address,
                       MachineType mach,
                       DataBlock& data);

    void writeCallback(Addr address,
                       MachineType mach,
                       DataBlock& data,
                       Cycles initialRequestTime,
                       Cycles forwardRequestTime,
                       Cycles firstResponseTime,
                       bool isRegion);

    void writeCallback(Addr address,
                       MachineType mach,
                       DataBlock& data,
                       Cycles initialRequestTime,
                       Cycles forwardRequestTime,
                       Cycles firstResponseTime);

    void readCallback(Addr address, DataBlock& data);

    void readCallback(Addr address,
                      MachineType mach,
                      DataBlock& data);

    void readCallback(Addr address,
                      MachineType mach,
                      DataBlock& data,
                      Cycles initialRequestTime,
                      Cycles forwardRequestTime,
                      Cycles firstResponseTime);

    void readCallback(Addr address,
                      MachineType mach,
                      DataBlock& data,
                      Cycles initialRequestTime,
                      Cycles forwardRequestTime,
                      Cycles firstResponseTime,
                      bool isRegion);

    bool coalescePacket(PacketPtr pkt) override;


  private:
    void invTCP();

    // current cache invalidation packet
    // nullptr if there is no active cache invalidation request
    PacketPtr m_cache_inv_pkt;

    // number of remaining cache lines to be invalidated in TCP
    int m_num_pending_invs;

protected:
	virtual RubyRequestType getRequestType(PacketPtr pkt);
};
#endif //__MEM_RUBY_SYSTEM_VIPERCOALESCER_HH__
