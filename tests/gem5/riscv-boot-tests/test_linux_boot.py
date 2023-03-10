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

import re

from testlib import *

if config.bin_path:
    resource_path = config.bin_path
else:
    resource_path = joinpath(absdirpath(__file__), "..", "resources")


def test_boot(
    cpu: str,
    num_cpus: int,
    to_tick: int,
    length: str,
):
    name = "{}-cpu_{}-cores_riscv-boot-test_to-tick".format(cpu, str(num_cpus))

    verifiers = []
    exit_regex = re.compile(
        "Exiting @ tick {} because simulate\(\) limit reached".format(
            str(to_tick)
        )
    )
    verifiers.append(verifier.MatchRegex(exit_regex))

    gem5_verify_config(
        name=name,
        verifiers=verifiers,
        fixtures=(),
        config=joinpath(
            config.base_dir,
            "tests",
            "gem5",
            "configs",
            "riscv_boot_exit_run.py",
        ),
        config_args=[
            "--cpu",
            cpu,
            "--num-cpus",
            str(num_cpus),
            "--tick-exit",
            str(to_tick),
            "--override-download",
            "--resource-directory",
            resource_path,
        ],
        valid_isas=(constants.riscv_tag,),
        valid_hosts=constants.supported_hosts,
        length=length,
    )


#### The quick (pre-submit/Kokoro) tests ####

test_boot(
    cpu="atomic",
    num_cpus=1,
    to_tick=10000000000,  # Simulates 1/100th of a second.
    length=constants.quick_tag,
)

test_boot(
    cpu="timing",
    num_cpus=1,
    to_tick=10000000000,
    length=constants.quick_tag,
)

test_boot(
    cpu="o3",
    num_cpus=1,
    to_tick=10000000000,
    length=constants.quick_tag,
)

test_boot(
    cpu="timing",
    num_cpus=4,
    to_tick=10000000000,
    length=constants.quick_tag,
)
