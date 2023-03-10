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


from ...resources.resource import AbstractResource
from ...utils.override import overrides
from .abstract_board import AbstractBoard
from ...isas import ISA

import m5
from m5.objects import (
    Cache,
    Pc,
    AddrRange,
    X86FsLinux,
    Addr,
    X86SMBiosBiosInformation,
    X86IntelMPProcessor,
    X86IntelMPIOAPIC,
    X86IntelMPBus,
    X86IntelMPBusHierarchy,
    X86IntelMPIOIntAssignment,
    X86E820Entry,
    Bridge,
    IOXBar,
    IdeDisk,
    CowDiskImage,
    RawDiskImage,
    BaseXBar,
    Port,
)

from m5.util.convert import toMemorySize


from .simple_board import SimpleBoard
from ..processors.abstract_processor import AbstractProcessor
from ..memory.abstract_memory_system import AbstractMemorySystem
from ..cachehierarchies.abstract_cache_hierarchy import AbstractCacheHierarchy
from ...utils.requires import requires

import os
from typing import List, Optional, Sequence


class X86Board(SimpleBoard):
    """
    A board capable of full system simulation for X86.

    **Limitations**
    * Currently, this board's memory is hardcoded to 3GB
    * Much of the I/O subsystem is hard coded
    """

    def __init__(
        self,
        clk_freq: str,
        processor: AbstractProcessor,
        memory: AbstractMemorySystem,
        cache_hierarchy: AbstractCacheHierarchy,
        exit_on_work_items: bool = False,
    ) -> None:
        super(X86Board, self).__init__(
            clk_freq=clk_freq,
            processor=processor,
            memory=memory,
            cache_hierarchy=cache_hierarchy,
            exit_on_work_items=exit_on_work_items,
        )

        requires(isa_required=ISA.X86)

        self.pc = Pc()

        self.workload = X86FsLinux()

        # North Bridge
        self.iobus = IOXBar()

    def _setup_io_devices(self):
        """ Sets up the x86 IO devices.

        Note: This is mostly copy-paste from prior X86 FS setups. Some of it
        may not be documented and there may be bugs.
        """

        # Constants similar to x86_traits.hh
        IO_address_space_base = 0x8000000000000000
        pci_config_address_space_base = 0xC000000000000000
        interrupts_address_space_base = 0xA000000000000000
        APIC_range_size = 1 << 12

        # Setup memory system specific settings.
        if self.get_cache_hierarchy().is_ruby():
            self.pc.attachIO(self.get_io_bus(), [self.pc.south_bridge.ide.dma])
        else:
            self.bridge = Bridge(delay="50ns")
            self.bridge.mem_side_port = self.get_io_bus().cpu_side_ports
            self.bridge.cpu_side_port = (
                self.get_cache_hierarchy().get_mem_side_port()
            )

            # # Constants similar to x86_traits.hh
            IO_address_space_base = 0x8000000000000000
            pci_config_address_space_base = 0xC000000000000000
            interrupts_address_space_base = 0xA000000000000000
            APIC_range_size = 1 << 12

            self.bridge.ranges = [
                AddrRange(0xC0000000, 0xFFFF0000),
                AddrRange(
                    IO_address_space_base, interrupts_address_space_base - 1
                ),
                AddrRange(pci_config_address_space_base, Addr.max),
            ]

            self.apicbridge = Bridge(delay="50ns")
            self.apicbridge.cpu_side_port = self.get_io_bus().mem_side_ports
            self.apicbridge.mem_side_port = (
                self.get_cache_hierarchy().get_cpu_side_port()
            )
            self.apicbridge.ranges = [
                AddrRange(
                    interrupts_address_space_base,
                    interrupts_address_space_base
                    + self.get_processor().get_num_cores() * APIC_range_size
                    - 1,
                )
            ]
            self.pc.attachIO(self.get_io_bus())

        # Add in a Bios information structure.
        self.workload.smbios_table.structures = [X86SMBiosBiosInformation()]

        # Set up the Intel MP table
        base_entries = []
        ext_entries = []
        for i in range(self.get_processor().get_num_cores()):
            bp = X86IntelMPProcessor(
                local_apic_id=i,
                local_apic_version=0x14,
                enable=True,
                bootstrap=(i == 0),
            )
            base_entries.append(bp)

        io_apic = X86IntelMPIOAPIC(
            id=self.get_processor().get_num_cores(),
            version=0x11,
            enable=True,
            address=0xFEC00000,
        )

        self.pc.south_bridge.io_apic.apic_id = io_apic.id
        base_entries.append(io_apic)
        pci_bus = X86IntelMPBus(bus_id=0, bus_type="PCI   ")
        base_entries.append(pci_bus)
        isa_bus = X86IntelMPBus(bus_id=1, bus_type="ISA   ")
        base_entries.append(isa_bus)
        connect_busses = X86IntelMPBusHierarchy(
            bus_id=1, subtractive_decode=True, parent_bus=0
        )
        ext_entries.append(connect_busses)

        pci_dev4_inta = X86IntelMPIOIntAssignment(
            interrupt_type="INT",
            polarity="ConformPolarity",
            trigger="ConformTrigger",
            source_bus_id=0,
            source_bus_irq=0 + (4 << 2),
            dest_io_apic_id=io_apic.id,
            dest_io_apic_intin=16,
        )

        base_entries.append(pci_dev4_inta)

        def assignISAInt(irq, apicPin):

            assign_8259_to_apic = X86IntelMPIOIntAssignment(
                interrupt_type="ExtInt",
                polarity="ConformPolarity",
                trigger="ConformTrigger",
                source_bus_id=1,
                source_bus_irq=irq,
                dest_io_apic_id=io_apic.id,
                dest_io_apic_intin=0,
            )
            base_entries.append(assign_8259_to_apic)

            assign_to_apic = X86IntelMPIOIntAssignment(
                interrupt_type="INT",
                polarity="ConformPolarity",
                trigger="ConformTrigger",
                source_bus_id=1,
                source_bus_irq=irq,
                dest_io_apic_id=io_apic.id,
                dest_io_apic_intin=apicPin,
            )
            base_entries.append(assign_to_apic)

        assignISAInt(0, 2)
        assignISAInt(1, 1)

        for i in range(3, 15):
            assignISAInt(i, i)

        self.workload.intel_mp_table.base_entries = base_entries
        self.workload.intel_mp_table.ext_entries = ext_entries

        entries = [
            # Mark the first megabyte of memory as reserved
            X86E820Entry(addr=0, size="639kB", range_type=1),
            X86E820Entry(addr=0x9FC00, size="385kB", range_type=2),
            # Mark the rest of physical memory as available
            X86E820Entry(
                addr=0x100000,
                size=f"{self.mem_ranges[0].size() - 0x100000:d}B",
                range_type=1,
            ),
        ]

        # Reserve the last 16kB of the 32-bit address space for m5ops
        entries.append(
            X86E820Entry(addr=0xFFFF0000, size="64kB", range_type=2)
        )

        self.workload.e820_table.entries = entries

    def connect_things(self) -> None:
        # This board is a bit particular about the order that things are
        # connected together.

        # Before incorporating the memory or creating the I/O devices figure
        # out the memory ranges.
        self.setup_memory_ranges()

        # Set up all of the I/O before we incorporate anything else.
        self._setup_io_devices()

        # Incorporate the cache hierarchy for the motherboard.
        self.get_cache_hierarchy().incorporate_cache(self)

        # Incorporate the processor into the motherboard.
        self.get_processor().incorporate_processor(self)

        # Incorporate the memory into the motherboard.
        self.get_memory().incorporate_memory(self)


    def set_workload(
        self,
        kernel: AbstractResource,
        disk_image: AbstractResource,
        command: Optional[str] = None,
        kernel_args: Optional[List[str]] = [],
    ):
        """Setup the full system files

        See <url> for the currently tested kernels and OSes.

        The command is an optional string to execute once the OS is fully
        booted, assuming the disk image is setup to run `m5 readfile` after
        booting.

        **Limitations**
        * Only supports a Linux kernel
        * Disk must be configured correctly to use the command option

        :param kernel: The compiled kernel binary resource
        :param disk_image: A disk image resource containing the OS data. The
            first partition should be the root partition.
        :param command: The command(s) to run with bash once the OS is booted
        :param kernel_args: Additional arguments to be passed to the kernel.
        `earlyprintk=ttyS0 console=ttyS0 lpj=7999923 root=/dev/hda1` are
        already passed. This parameter is used to pass additional arguments.
        """

        # Set the Linux kernel to use.
        self.workload.object_file = kernel.get_local_path()

        # Options specified on the kernel command line.
        self.workload.command_line = " ".join(
            [
                "earlyprintk=ttyS0",
                "console=ttyS0",
                "lpj=7999923",
                "root=/dev/hda1",
            ] + kernel_args
        )

        # Create the Disk image SimObject.
        ide_disk = IdeDisk()
        ide_disk.driveID = "device0"
        ide_disk.image = CowDiskImage(
            child=RawDiskImage(read_only=True), read_only=False
        )
        ide_disk.image.child.image_file = disk_image.get_local_path()

        # Attach the SimObject to the system.
        self.pc.south_bridge.ide.disks = [ide_disk]

        # Set the script to be passed to the simulated system to execute after
        # boot.
        if command:
            file_name = os.path.join(m5.options.outdir, "run")
            bench_file = open(file_name, "w+")
            bench_file.write(command)
            bench_file.close()

            # Set to the system readfile
            self.readfile = file_name

    @overrides(AbstractBoard)
    def has_io_bus(self) -> bool:
        return True

    @overrides(AbstractBoard)
    def get_io_bus(self) -> BaseXBar:
        return self.iobus

    @overrides(AbstractBoard)
    def has_dma_ports(self) -> bool:
        return True

    @overrides(AbstractBoard)
    def get_dma_ports(self) -> Sequence[Port]:
        return [self.pc.south_bridge.ide.dma, self.iobus.mem_side_ports]

    @overrides(AbstractBoard)
    def has_coherent_io(self) -> bool:
        return True

    @overrides(AbstractBoard)
    def get_mem_side_coherent_io_port(self) -> Port:
        return self.iobus.mem_side_ports

    @overrides(AbstractBoard)
    def setup_memory_ranges(self):
        memory = self.get_memory()

        if memory.get_size() > toMemorySize("3GB"):
            raise Exception(
                "X86Board currently only supports memory sizes up "
                "to 3GB because of the I/O hole."
            )
        data_range = AddrRange(memory.get_size())
        memory.set_memory_range([data_range])

        # Add the address range for the IO
        self.mem_ranges = [
            data_range,  # All data
            AddrRange(0xC0000000, size=0x100000),  # For I/0
        ]
