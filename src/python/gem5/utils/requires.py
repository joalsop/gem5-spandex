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

from ..runtime import get_runtime_coherence_protocol, get_runtime_isa
from ..isas import ISA
from ..coherence_protocol import CoherenceProtocol
from typing import Optional
import os
import inspect


def _get_exception_str(msg: str):
    # The inspect module allows us to get information about live objects,
    # modules, classes, etc. Here was use it to generate an exception string
    # that incorporates information on what file or class this requirement was
    # stated. `inspect.stack()[1]` is the `requires` caller method. One above
    # this on the stack, `inspect.stack()[2]` should be where `requires` is
    # called.
    if inspect.stack()[2].function  == '<module>':
        # If the caller is a Python module, we use the filename. This is for
        # the case where the `requires` function is called outside of a class.
        name = inspect.stack()[2].filename
    else:
        # Otherwise we assume the `requires` is being called by a class, in
        # which case we label the exception message with the class name.
        name = inspect.stack()[2].frame.f_locals['self'].__class__.__name__
    return "[{}] {}".format(name, msg)


def requires(
    isa_required: Optional[ISA] = None,
    coherence_protocol_required: Optional[CoherenceProtocol] = None,
    kvm_required: Optional[bool] = False,
) -> None:
    """
    Ensures the ISA/Coherence protocol/KVM requirements are met. An exception
    will be raise if they are not.

    :param isa_required: The ISA gem5 must be compiled to.
    :param coherence_protocol_required: The coherence protocol gem5 must be
        compiled to.
    :param kvm_required: The host system must have the Kernel-based Virtual
        Machine available.
    :raises Exception: Raises an exception if the required ISA or coherence
        protocol do not match that of the current gem5 binary.
    """

    runtime_isa = get_runtime_isa()
    runtime_coherence_protocol = get_runtime_coherence_protocol()
    kvm_available = os.access("/dev/kvm", mode=os.R_OK | os.W_OK)

    if isa_required != None and isa_required != runtime_isa:
        raise Exception(
            _get_exception_str(
                msg="The current ISA is '{}'. Required: '{}'".format(
                    runtime_isa.name, isa_required.name
                )
            )
        )

    if (
        coherence_protocol_required != None
        and coherence_protocol_required != runtime_coherence_protocol
    ):
        raise Exception(
            _get_exception_str(
                msg="The current coherence protocol is "
                    "'{}'. Required: '{}'".format(
                        runtime_coherence_protocol.name,
                        coherence_protocol_required.name,
                )
            )
        )

    if kvm_required and not kvm_available:
        raise Exception(
            _get_exception_str(
                msg="KVM is required but is unavaiable on this system"
            )
        )