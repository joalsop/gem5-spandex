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

from m5.objects import (
    SrcClockDomain,
    ClockDomain,
    VoltageDomain,
    Port,
    IOXBar,
    AddrRange,
)

from .mem_mode import MemMode, mem_mode_to_string
from ...utils.override import overrides
from .abstract_board import AbstractBoard
from ..processors.abstract_processor import AbstractProcessor
from ..memory.abstract_memory_system import AbstractMemorySystem
from ..cachehierarchies.abstract_cache_hierarchy import AbstractCacheHierarchy


from typing import List


class TestBoard(AbstractBoard):

    """This is a Testing Board used to run traffic generators on a simple
    architecture.

    To work as a traffic generator board, pass a generator as a processor.
    """

    def __init__(
        self,
        clk_freq: str,
        processor: AbstractProcessor,
        memory: AbstractMemorySystem,
        cache_hierarchy: AbstractCacheHierarchy,
    ):
        super(TestBoard, self).__init__(
            processor=processor,
            memory=memory,
            cache_hierarchy=cache_hierarchy,
        )
        self.clk_domain = SrcClockDomain(
            clock=clk_freq, voltage_domain=VoltageDomain()
        )

    def connect_system_port(self, port: Port) -> None:
        self.system_port = port

    def connect_things(self) -> None:
        self.setup_memory_ranges()

        self.get_cache_hierarchy().incorporate_cache(self)

        self.get_processor().incorporate_processor(self)

        self.get_memory().incorporate_memory(self)

    def get_clock_domain(self) -> ClockDomain:
        return self.clk_domain

    @overrides(AbstractBoard)
    def has_io_bus(self) -> bool:
        return False

    @overrides(AbstractBoard)
    def get_io_bus(self) -> IOXBar:
        raise NotImplementedError(
            "The TestBoard does not have an IO Bus. "
            "Use `has_io_bus()` to check this."
        )

    @overrides(AbstractBoard)
    def get_dma_ports(self) -> List[Port]:
        return False

    @overrides(AbstractBoard)
    def get_dma_ports(self) -> List[Port]:
        raise NotImplementedError(
            "The TestBoard does not have DMA Ports. "
            "Use `has_dma_ports()` to check this."
        )

    @overrides(AbstractBoard)
    def has_coherent_io(self) -> bool:
        return False

    @overrides(AbstractBoard)
    def get_mem_side_coherent_io_port(self):
        raise NotImplementedError(
            "SimpleBoard does not have any I/O ports. Use has_coherent_io to "
            "check this."
        )

    @overrides(AbstractBoard)
    def set_mem_mode(self, mem_mode: MemMode) -> None:
        self.mem_mode = mem_mode_to_string(mem_mode=mem_mode)

    @overrides(AbstractBoard)
    def setup_memory_ranges(self) -> None:
        memory = self.get_memory()

        # The simple board just has one memory range that is the size of the
        # memory.
        self.mem_ranges = [AddrRange(memory.get_size())]
        memory.set_memory_range(self.mem_ranges)
