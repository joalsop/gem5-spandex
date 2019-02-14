#
#  Copyright (c) 2011-2015 Advanced Micro Devices, Inc.
#  All rights reserved.
#
#  For use for simulation and test purposes only
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#  this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright notice,
#  this list of conditions and the following disclaimer in the documentation
#  and/or other materials provided with the distribution.
#
#  3. Neither the name of the copyright holder nor the names of its
#  contributors may be used to endorse or promote products derived from this
#  software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#
#  Author: Lisa Hsu
#

import math
import m5
from m5.objects import *
from m5.defines import buildEnv
from Ruby import create_topology
from Ruby import send_evicts

from topologies.Cluster import Cluster
from topologies.Crossbar import Crossbar


class CntrlBase:
    _seqs = 0
    @classmethod
    def seqCount(cls):
        # Use SeqCount not class since we need global count
        CntrlBase._seqs += 1
        return CntrlBase._seqs - 1

    _cntrls = 0
    @classmethod
    def cntrlCount(cls):
        # Use CntlCount not class since we need global count
        CntrlBase._cntrls += 1
        return CntrlBase._cntrls - 1

    _version = 0
    @classmethod
    def versionCount(cls):
        cls._version += 1 # Use count for this particular type
        return cls._version - 1

class L1Cache(RubyCache):
    resourceStalls = False
    dataArrayBanks = 2
    tagArrayBanks = 2
    dataAccessLatency = 1
    tagAccessLatency = 1
    def create(self, size, assoc, options):
        self.size = MemorySize(size)
        self.assoc = assoc
        self.replacement_policy = PseudoLRUReplacementPolicy()

class ICache(RubyCache):
    resourceStalls = False
    dataArrayBanks = 2
    tagArrayBanks = 2
    dataAccessLatency = 1
    tagAccessLatency = 1
    def create(self, size, assoc, options):
        self.size = MemorySize(size)
        self.assoc = assoc
        self.replacement_policy = PseudoLRUReplacementPolicy()

class L2Cache(RubyCache):
    resourceStalls = False
    assoc = 16
    dataArrayBanks = 16
    tagArrayBanks = 16
    def create(self, size, assoc, options):
        self.size = MemorySize(size)
        self.assoc = assoc
        self.replacement_policy = PseudoLRUReplacementPolicy()

class TCPCache(RubyCache):
    size = "16kB"
    assoc = 16
    dataArrayBanks = 16 #number of data banks
    tagArrayBanks = 16  #number of tag banks
    dataAccessLatency = 4
    tagAccessLatency = 1
    def create(self, options):
        self.size = MemorySize(options.tcp_size)
        self.assoc = options.tcp_assoc
        self.resourceStalls = options.no_tcc_resource_stalls
        self.replacement_policy = PseudoLRUReplacementPolicy()

class SQCCache(RubyCache):
    dataArrayBanks = 8
    tagArrayBanks = 8
    dataAccessLatency = 1
    tagAccessLatency = 1

    def create(self, options):
        self.size = MemorySize(options.sqc_size)
        self.assoc = options.sqc_assoc
        self.replacement_policy = PseudoLRUReplacementPolicy()

class TCC(RubyCache):
    size = MemorySize("256kB")
    assoc = 16
    dataAccessLatency = 8
    tagAccessLatency = 2
    resourceStalls = True
    def create(self, options):
        self.assoc = options.tcc_assoc
        if hasattr(options, 'bw_scalor') and options.bw_scalor > 0:
          s = options.num_compute_units
          tcc_size = s * 128
          tcc_size = str(tcc_size)+'kB'
          self.size = MemorySize(tcc_size)
          self.dataArrayBanks = 64
          self.tagArrayBanks = 64
        else:
          self.size = MemorySize(options.tcc_size)
          self.dataArrayBanks = 256 / options.num_tccs #number of data banks
          self.tagArrayBanks = 256 / options.num_tccs #number of tag banks
        self.size.value = self.size.value / options.num_tccs
        if ((self.size.value / long(self.assoc)) < 128):
            self.size.value = long(128 * self.assoc)
        self.start_index_bit = math.log(options.cacheline_size, 2) + \
                               math.log(options.num_tccs, 2)
        self.replacement_policy = PseudoLRUReplacementPolicy()

class ICntrl(ICache_Controller, CntrlBase):
    def create(self, options, ruby_system, system, cache_type):
        self.version = self.versionCount()

        if cache_type == "ICACHE":
            self.L1cache = ICache()
            self.L1cache.create(options.l1i_size, options.l1i_assoc, options)
        elif cache_type == "SQC":
            self.L1cache = SQCCache()
            self.L1cache.create(options)
        else:
            panic("Unsupported instruction cache type")

        self.sequencer = RubySequencer()

        self.sequencer.version = self.seqCount()
        self.sequencer.icache = self.L1cache
        self.sequencer.dcache = self.L1cache
        self.sequencer.ruby_system = ruby_system

        self.sequencer.support_data_reqs = False
        self.sequencer.is_cpu_sequencer = False

        self.ruby_system = ruby_system

        if options.recycle_latency:
            self.recycle_latency = options.recycle_latency

class L1Cntrl(L1Cache_Controller, CntrlBase):
    def create(self, options, ruby_system, sysyem, cache_type):
        self.version = self.versionCount()

        if cache_type == "L1":
            print("WHD: Create L1")
            self.L1cache = L1Cache()
            self.L1cache.create(options.l1d_size, options.l1d_assoc, options)
            self.use_seq_not_coal = True
        elif cache_type == "TCP":
            print("WHD: Create TCP")
            self.L1cache = TCPCache(tagAccessLatency = options.TCP_latency,
                                    dataAccessLatency = options.TCP_latency)
            self.L1cache.resourceStalls = options.no_resource_stalls
            self.L1cache.create(options)
            self.issue_latency = 1
            self.use_seq_not_coal = False
        elif cache_type == "CP":
            print("WHD: Create CP")
            self.L1cache = TCPCache(tagAccessLatency = options.TCP_latency,
                                    dataAccessLatency = options.TCP_latency)
            self.L1cache.resourceStalls = options.no_resource_stalls
            self.L1cache.create(options)
            self.issue_latency = 1
            self.use_seq_not_coal = True
        else:
            panic("Unsupported first level cache type")

        self.coalescer = DeNovoCoalescer()
        self.coalescer.version = self.seqCount()
        self.coalescer.icache = self.L1cache
        self.coalescer.dcache = self.L1cache
        self.coalescer.ruby_system = ruby_system
        self.coalescer.support_inst_reqs = False
        self.coalescer.is_cpu_sequencer = False
        self.coalescer.max_coalesces_per_cycle = \
            options.max_coalesces_per_cycle

        self.sequencer = SpandexSequencer()
        self.sequencer.version = self.seqCount()
        self.sequencer.icache = self.L1cache
        self.sequencer.dcache = self.L1cache
        self.sequencer.ruby_system = ruby_system
        self.sequencer.is_cpu_sequencer = True

        self.ruby_system = ruby_system
        if options.recycle_latency:
            self.recycle_latency = options.recycle_latency

class L2Cntrl(L2Cache_Controller, CntrlBase):
    def create(self, options, dir_ranges, ruby_system, system, cache_type):
        self.version = self.versionCount()

        if cache_type == "L2":
            self.L2cache = L2Cache()
            self.L2cache.create(options.l2_size, options.l2_assoc, options)
        elif cache_type == "TCC":
            self.L2cache = TCC()
            self.L2cache.create(options)
            self.L2cache.resourceStalls = options.no_tcc_resource_stalls
        else:
            panic("Unsupported second level cache type")

        self.addr_ranges = dir_ranges
        self.ruby_system = ruby_system
        self.directory = RubyDirectoryMemory()

        if options.recycle_latency:
            self.recycle_latency = options.recycle_latency

def define_options(parser):
    parser.add_option("--num-subcaches", type = "int", default = 4)
    parser.add_option("--l3-data-latency", type = "int", default = 20)
    parser.add_option("--l3-tag-latency", type = "int", default = 15)
    parser.add_option("--cpu-to-dir-latency", type = "int", default = 120)
    parser.add_option("--gpu-to-dir-latency", type = "int", default = 120)
    parser.add_option("--no-resource-stalls", action = "store_false",
                      default = True)
    parser.add_option("--no-tcc-resource-stalls", action = "store_false",
                      default = True)
    parser.add_option("--use-L3-on-WT", action = "store_true", default = False)
    parser.add_option("--num-tbes", type = "int", default = 256)
    parser.add_option("--l2-latency", type = "int", default = 50) # load to use
    parser.add_option("--num-tccs", type = "int", default = 1,
                      help = "number of TCC banks in the GPU")
    parser.add_option("--sqc-size", type = 'string', default = '32kB',
                      help = "SQC cache size")
    parser.add_option("--sqc-assoc", type = 'int', default = 8,
                      help = "SQC cache assoc")
    parser.add_option("--WB_L1", action = "store_true", default = False,
                      help = "writeback L1")
    parser.add_option("--WB_L2", action = "store_true", default = False,
                      help = "writeback L2")
    parser.add_option("--TCP_latency", type = "int", default = 4,
                      help = "TCP latency")
    parser.add_option("--TCC_latency", type = "int", default = 16,
                      help = "TCC latency")
    parser.add_option("--tcc-size", type = 'string', default = '256kB',
                      help = "agregate tcc size")
    parser.add_option("--tcc-assoc", type = 'int', default = 16,
                      help = "tcc assoc")
    parser.add_option("--tcp-size", type = 'string', default = '16kB',
                      help = "tcp size")
    parser.add_option("--tcp-assoc", type = 'int', default = 16,
                      help = "tcp assoc")
    parser.add_option("--max-coalesces-per-cycle", type="int", default=1,
                      help="Maximum insts that may coalesce in a cycle");

    parser.add_option("--noL1", action = "store_true", default = False,
                      help = "bypassL1")
    parser.add_option("--buffers-size", type="int", default=128,
                      help="Size of MessageBuffers at the controller")

def create_system(options, full_system, system, dma_devices, ruby_system):
    if buildEnv['PROTOCOL'] not in ('DeNovo', 'Spandex'):
        panic("This script requires the DeNovo protocol to be built.")

    cpu_sequencers = []

    #
    # The ruby network creation expects the list of nodes in the system to be
    # consistent with the NetDest list.  Therefore the l1 controller nodes
    # must be listed before the directory nodes and directory nodes before
    # dma nodes, etc.
    #
    l1_cntrl_nodes      = []
    icache_cntrl_nodes  = []
    l2_cntrl_nodes      = []
    tcp_cntrl_nodes     = []
    sqc_cntrl_nodes     = []
    tcc_cntrl_nodes     = []
    dir_cntrl_nodes     = []

    #
    # Must create the individual controllers before the network to ensure the
    # controller constructors are called before the network constructor
    #

    # For an odd number of CPUs, still create the right number of controllers
    DIR_bits = int(math.log(options.num_dirs, 2))

    # This is the base crossbar that connects the L3s, Dirs, and cpu/gpu
    # Clusters
    crossbar_bw = None
    mainCluster = None
    if hasattr(options, 'bw_scalor') and options.bw_scalor > 0:
        #Assuming a 2GHz clock
        crossbar_bw = 16 * options.num_compute_units * options.bw_scalor
        mainCluster = Cluster(intBW=crossbar_bw)
    else:
        mainCluster = Cluster(intBW=8) # 16 GB/s

    # See comment in config/common/MemConfig.py for explanation of this value
    xor_low_bit = 20

    if options.numa_high_bit:
        numa_bit = options.numa_high_bit
        dir_bits = int(math.log(options.num_dirs, 2))
        xor_high_bit = xor_low_bit + dir_bits - 1
    else:
        # if the numa_bit is not specified, set the directory bits as the
        # lowest bits above the block offset bits, and the numa_bit as the
        # highest of those directory bits
        dir_bits = int(math.log(options.num_dirs, 2))
        block_size_bits = int(math.log(options.cacheline_size, 2))
        numa_bit = block_size_bits + dir_bits - 1
        xor_high_bit = xor_low_bit + dir_bits - 1

    for i in xrange( options.num_dirs):
        dir_ranges = []
        for r in system.mem_ranges:
            addr_range = m5.objects.AddrRange(r.start, size=r.size(),
                    intlvHighBit = numa_bit,
                    intlvBits = dir_bits,
                    intlvMatch = i,
                    xorHighBit = xor_high_bit)
            dir_ranges.append(addr_range)

        l2_cntrl = L2Cntrl()
        l2_cntrl.create(options, dir_ranges, ruby_system, system, "L2")

        # Connect the CP controllers and the network
        l2_cntrl.requestFromL1= MessageBuffer(ordered = True)
        l2_cntrl.requestFromL1.slave = ruby_system.network.master

        l2_cntrl.requestToL1= MessageBuffer(ordered = True)
        l2_cntrl.requestToL1.master = ruby_system.network.slave

        l2_cntrl.responseToL1= MessageBuffer(ordered = True)
        l2_cntrl.responseToL1.master = ruby_system.network.slave

        l2_cntrl.responseFromL1= MessageBuffer(ordered = True)
        l2_cntrl.responseFromL1.slave = ruby_system.network.master

        l2_cntrl.requestToMemory = MessageBuffer()
        l2_cntrl.responseFromMemory = MessageBuffer()

        exec("ruby_system.l2_cntrl%d = l2_cntrl" % i)
        dir_cntrl_nodes.append(l2_cntrl)
        mainCluster.add(l2_cntrl)

    cpuCluster = None
    if hasattr(options, 'bw_scalor') and options.bw_scalor > 0:
        cpuCluster = Cluster(extBW = crossbar_bw, intBW = crossbar_bw)
    else:
        cpuCluster = Cluster(extBW = 8, intBW = 8) # 16 GB/s
    for i in xrange( options.num_cpus ):
        l1_cntrl= L1Cntrl()
        l1_cntrl.create(options, ruby_system, system, "L1")

        exec("ruby_system.l1_cntrl%d = l1_cntrl" % i)
        #
        # Add controllers and sequencers to the appropriate lists
        #
        cpu_sequencers.append(l1_cntrl.sequencer)

        # Connect the CP controllers and the network
        l1_cntrl.requestFromL1Cache = \
                MessageBuffer(buffer_size=options.buffers_size)
        l1_cntrl.requestFromL1Cache.master = ruby_system.network.slave

        l1_cntrl.requestToL1Cache = \
                MessageBuffer(buffer_size=options.buffers_size)
        l1_cntrl.requestToL1Cache.slave = ruby_system.network.master

        l1_cntrl.responseToL1Cache = \
                MessageBuffer(buffer_size=options.buffers_size)
        l1_cntrl.responseToL1Cache.slave = ruby_system.network.master

        l1_cntrl.responseFromL1Cache = \
                MessageBuffer(buffer_size=options.buffers_size)
        l1_cntrl.responseFromL1Cache.master = ruby_system.network.slave

        l1_cntrl.peerRequestFromL1Cache = \
                MessageBuffer(buffer_size=options.buffers_size)
        l1_cntrl.peerRequestFromL1Cache.slave = ruby_system.network.master

        l1_cntrl.peerRequestToL1Cache = \
                MessageBuffer(buffer_size=options.buffers_size)
        l1_cntrl.peerRequestToL1Cache.master = ruby_system.network.slave

        l1_cntrl.peerResponseFromL1Cache = \
                MessageBuffer(buffer_size=options.buffers_size)
        l1_cntrl.peerResponseFromL1Cache.slave = ruby_system.network.master

        l1_cntrl.peerResponseToL1Cache = \
                MessageBuffer(buffer_size=options.buffers_size)
        l1_cntrl.peerResponseToL1Cache.master = ruby_system.network.slave

        l1_cntrl.mandatoryQueue = \
            MessageBuffer(buffer_size=options.buffers_size)

        cpuCluster.add(l1_cntrl)

    # WHD: TMP change to cut in half
    #for i in xrange( options.num_cpus ):
    #    icache_cntrl= ICntrl()
    #    icache_cntrl.create(options, ruby_system, system, "ICACHE")

    #    cpu_sequencers.append(icache_cntrl.sequencer)

    #    exec("ruby_system.icache_cntrl%d = icache_cntrl" % i)
    #    #
    #    # Add controllers and sequencers to the appropriate lists
    #    #

    #    # Connect the CP controllers and the network
    #    icache_cntrl.mandatoryQueue = \
    #        MessageBuffer(buffer_size=options.buffers_size)

    #    cpuCluster.add(icache_cntrl)



    gpuCluster = None
    if hasattr(options, 'bw_scalor') and options.bw_scalor > 0:
      gpuCluster = Cluster(extBW = crossbar_bw, intBW = crossbar_bw)
    else:
      gpuCluster = Cluster(extBW = 8, intBW = 8) # 16 GB/s
    for i in xrange(options.num_compute_units):

        #tcp_cntrl = L1Cntrl(L2_select_num_bits = DIR_bits,
        #                     issue_latency = 1,
        #                     number_of_TBEs = 2560)

        tcp_cntrl = L1Cntrl( issue_latency = 1,
                             number_of_TBEs = 2560)
        # TBEs set to max outstanding requests
        tcp_cntrl.create(options, ruby_system, system, "TCP")
        #tcp_cntrl.WB = options.WB_L1
        #tcp_cntrl.disableL1 = options.noL1
        tcp_cntrl.L1cache.tagAccessLatency = options.TCP_latency
        tcp_cntrl.L1cache.dataAccessLatency = options.TCP_latency

        exec("ruby_system.tcp_cntrl%d = tcp_cntrl" % i)
        #
        # Add controllers and sequencers to the appropriate lists
        #
        cpu_sequencers.append(tcp_cntrl.coalescer)
        tcp_cntrl_nodes.append(tcp_cntrl)

        # Connect the TCP controller to the ruby network
        tcp_cntrl.requestFromL1Cache = MessageBuffer(ordered = True)
        tcp_cntrl.requestFromL1Cache.master = ruby_system.network.slave

        tcp_cntrl.requestToL1Cache = MessageBuffer()
        tcp_cntrl.requestToL1Cache.slave = ruby_system.network.master

        tcp_cntrl.responseToL1Cache = MessageBuffer(ordered = True)
        tcp_cntrl.responseToL1Cache.slave = ruby_system.network.master

        tcp_cntrl.responseFromL1Cache = MessageBuffer()
        tcp_cntrl.responseFromL1Cache.master = ruby_system.network.slave

        tcp_cntrl.peerRequestFromL1Cache = MessageBuffer(ordered = True)
        tcp_cntrl.peerRequestFromL1Cache.slave = ruby_system.network.master

        tcp_cntrl.peerRequestToL1Cache = MessageBuffer(ordered = True)
        tcp_cntrl.peerRequestToL1Cache.master = ruby_system.network.slave

        tcp_cntrl.peerResponseFromL1Cache = MessageBuffer(ordered = True)
        tcp_cntrl.peerResponseFromL1Cache.slave = ruby_system.network.master

        tcp_cntrl.peerResponseToL1Cache = MessageBuffer(ordered = True)
        tcp_cntrl.peerResponseToL1Cache.master = ruby_system.network.slave

        tcp_cntrl.mandatoryQueue = \
            MessageBuffer(buffer_size=options.buffers_size)

        gpuCluster.add(tcp_cntrl)

    for i in xrange(options.num_sqc):

        sqc_cntrl = ICntrl()
        sqc_cntrl.create(options, ruby_system, system, "SQC")

        exec("ruby_system.sqc_cntrl%d = sqc_cntrl" % i)
        #
        # Add controllers and sequencers to the appropriate lists
        #
        cpu_sequencers.append(sqc_cntrl.sequencer)

        # Connect the SQC controller to the ruby network
        sqc_cntrl.mandatoryQueue = \
            MessageBuffer(buffer_size=options.buffers_size)

        # SQC also in GPU cluster
        gpuCluster.add(sqc_cntrl)

    for i in xrange(options.num_scalar_cache):
        scalar_cntrl = ICntrl()
        scalar_cntrl.create(options, ruby_system, system, "SQC")

        exec('ruby_system.scalar_cntrl%d = scalar_cntrl' % i)

        cpu_sequencers.append(scalar_cntrl.sequencer)

        scalar_cntrl.mandatoryQueue = \
            MessageBuffer(buffer_size=options.buffers_size)

        gpuCluster.add(scalar_cntrl)

    for i in xrange(options.num_cp):

        tcp_ID = options.num_compute_units + i
        sqc_ID = options.num_sqc + i

        tcp_cntrl = L1Cntrl(L2_select_num_bits = DIR_bits,
                             issue_latency = 1,
                             number_of_TBEs = 2560)
        # TBEs set to max outstanding requests
        tcp_cntrl.create(options, ruby_system, system, "CP")
        #tcp_cntrl.WB = options.WB_L1
        #tcp_cntrl.disableL1 = options.noL1
        tcp_cntrl.L1cache.tagAccessLatency = options.TCP_latency
        tcp_cntrl.L1cache.dataAccessLatency = options.TCP_latency

        exec("ruby_system.tcp_cntrl%d = tcp_cntrl" % tcp_ID)
        #
        # Add controllers and sequencers to the appropriate lists
        #
        cpu_sequencers.append(tcp_cntrl.sequencer)
        tcp_cntrl_nodes.append(tcp_cntrl)

        # Connect the CP (TCP) controllers to the ruby network
        tcp_cntrl.requestFromL1Cache = MessageBuffer(ordered = True)
        tcp_cntrl.requestFromL1Cache.master = ruby_system.network.slave

        tcp_cntrl.responseToL1Cache = MessageBuffer(ordered = True)
        tcp_cntrl.responseToL1Cache.slave = ruby_system.network.master

        tcp_cntrl.mandatoryQueue = \
            MessageBuffer(buffer_size=options.buffers_size)

        gpuCluster.add(tcp_cntrl)

        sqc_cntrl = ICntrl()
        sqc_cntrl.create(options, ruby_system, system, "ICACHE")
        sqc_cntrl.mandatoryQueue = \
                MessageBuffer(buffer_size=options.buffers_size)
        exec("ruby_system.sqc_cntrl%d = sqc_cntrl" % sqc_ID)
        #
        # Add controllers and sequencers to the appropriate lists
        #
        cpu_sequencers.append(sqc_cntrl.sequencer)

        # SQC also in GPU cluster
        gpuCluster.add(sqc_cntrl)

#    for i in xrange(options.num_tccs):
#
#        tcc_cntrl = L2Cntrl(l2_response_latency = options.TCC_latency)
#        tcc_cntrl.create(options, ruby_system, system, "L2")
#        tcc_cntrl.l2_request_latency = options.gpu_to_dir_latency
#        tcc_cntrl.l2_response_latency = options.TCC_latency
#        tcc_cntrl_nodes.append(tcc_cntrl)
#        # the number_of_TBEs is inclusive of TBEs below
#
#        # Connect the TCC controllers to the ruby network
#        tcc_cntrl.requestFromL1 = MessageBuffer(ordered = True)
#        tcc_cntrl.requestFromL1.slave = ruby_system.network.master
#
#        tcc_cntrl.responseToL1 = MessageBuffer(ordered = True)
#        tcc_cntrl.responseToL1.master = ruby_system.network.slave
#
#        tcc_cntrl.requestToMemory = MessageBuffer()
#        tcc_cntrl.responseFromMemory = MessageBuffer()
#
#        exec("ruby_system.tcc_cntrl%d = tcc_cntrl" % i)
#
#        # connect all of the wire buffers between L3 and dirs up
#        # TCC cntrls added to the GPU cluster
#        dir_cntrl_nodes.append(tcc_cntrl)
#        gpuCluster.add(tcc_cntrl)

    for i, dma_device in enumerate(dma_devices):
        dma_seq = DMASequencer(version=i, ruby_system=ruby_system)
        dma_cntrl = DMA_Controller(version=i, dma_sequencer=dma_seq,
                                   ruby_system=ruby_system)
        exec('system.dma_cntrl%d = dma_cntrl' % i)
        if dma_device.type == 'MemTest':
            exec('system.dma_cntrl%d.dma_sequencer.slave = dma_devices.test'
                 % i)
        else:
            exec('system.dma_cntrl%d.dma_sequencer.slave = dma_device.dma' % i)
        dma_cntrl.requestToDir = MessageBuffer(buffer_size=0)
        dma_cntrl.requestToDir.master = ruby_system.network.slave
        dma_cntrl.responseFromDir = MessageBuffer(buffer_size=0)
        dma_cntrl.responseFromDir.slave = ruby_system.network.master
        dma_cntrl.mandatoryQueue = MessageBuffer(buffer_size = 0)
        gpuCluster.add(dma_cntrl)

    # Add cpu/gpu clusters to main cluster
    mainCluster.add(cpuCluster)
    mainCluster.add(gpuCluster)

    ruby_system.network.number_of_virtual_networks = 11
    return (cpu_sequencers, dir_cntrl_nodes, mainCluster)
