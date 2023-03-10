# Copyright (c) 2021 The Regents of the University of California
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
This example runs a simple linux boot on the RiscvBoard.

Characteristics
---------------

* Runs exclusively on the RISC-V ISA with the classic caches
"""

import m5
from m5.objects import Root

from gem5.components.memory.single_channel import SingleChannelDDR3_1600
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.components.processors.cpu_types import CPUTypes
from gem5.isas import ISA
from gem5.utils.requires import requires
from gem5.resources.resource import Resource

import argparse

parser = argparse.ArgumentParser(
    description="A script to run the RISCV boot exit tests."
)

parser.add_argument(
    "-n",
    "--num-cpus",
    type=int,
    required=True,
    help="The number of CPUs.",
)

parser.add_argument(
    "-c",
    "--cpu",
    type=str,
    choices=("kvm", "atomic", "timing", "o3"),
    required=True,
    help="The CPU type.",
)

parser.add_argument(
    "-t",
    "--tick-exit",
    type=int,
    required=True,
    help="The tick to exit the simulation.",
)

parser.add_argument(
    "-r",
    "--resource-directory",
    type=str,
    required=False,
    help="The directory in which resources will be downloaded or exist.",
)

parser.add_argument(
    "-o",
    "--override-download",
    action="store_true",
    help="Override a local resource if the hashes do not match.",
)

args = parser.parse_args()

# Run a check to ensure the right version of gem5 is being used.
requires(isa_required=ISA.RISCV)

from gem5.components.cachehierarchies.classic.\
    private_l1_private_l2_cache_hierarchy import \
        PrivateL1PrivateL2CacheHierarchy
from gem5.components.boards.riscv_board import RiscvBoard

# Setup the cache hierarchy.
cache_hierarchy = PrivateL1PrivateL2CacheHierarchy(
    l1d_size="32KiB", l1i_size="32KiB", l2_size="512KiB"
)

# Setup the system memory.
memory = SingleChannelDDR3_1600()

# Setup a processor.
if args.cpu == "kvm":
    cpu_type = CPUTypes.KVM
elif args.cpu == "atomic":
    cpu_type = CPUTypes.ATOMIC
elif args.cpu == "timing":
    cpu_type = CPUTypes.TIMING
elif args.cpu == "o3":
    cpu_type = CPUTypes.O3
else:
    raise NotImplementedError(
        "CPU type '{}' is not supported in the boot tests.".format(args.cpu)
    )

processor = SimpleProcessor(cpu_type=cpu_type, num_cores=args.num_cpus)

# Setup the board.
board = RiscvBoard(
    clk_freq="1GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)

board.connect_things()

# Set the Full System workload.
board.set_workload(
    disk_image=Resource(
        "riscv-disk-img",
        override=args.override_download,
        resource_directory=args.resource_directory,
    ),
    bootloader=Resource(
        "riscv-bootloader-vmlinux-5.10",
        override=args.override_download,
        resource_directory=args.resource_directory,
    ),
)

root = Root(full_system=True, system=board)

m5.instantiate()

exit_event = m5.simulate(args.tick_exit)
print(
    "Exiting @ tick {} because {}.".format(m5.curTick(), exit_event.getCause())
)
