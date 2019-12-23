/*
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ARCH_SPARC_FS_WORKLOAD_HH__
#define __ARCH_SPARC_FS_WORKLOAD_HH__

#include "base/loader/symtab.hh"
#include "cpu/pc_event.hh"
#include "kern/system_events.hh"
#include "params/SparcFsWorkload.hh"
#include "sim/os_kernel.hh"
#include "sim/sim_object.hh"

namespace SparcISA
{

class FsWorkload : public OsKernel
{
  public:
    typedef SparcFsWorkloadParams Params;
    FsWorkload(Params *p);
    ~FsWorkload();

    void initState() override;

/**
 * Serialization stuff
 */
  public:
    void serializeSymtab(CheckpointOut &cp) const override;
    void unserializeSymtab(CheckpointIn &cp) override;

    /** reset binary symbol table */
    SymbolTable *resetSymtab;

    /** hypervison binary symbol table */
    SymbolTable *hypervisorSymtab;

    /** openboot symbol table */
    SymbolTable *openbootSymtab;

    /** nvram symbol table? */
    SymbolTable *nvramSymtab;

    /** hypervisor desc symbol table? */
    SymbolTable *hypervisorDescSymtab;

    /** partition desc symbol table? */
    SymbolTable *partitionDescSymtab;

    /** Object pointer for the reset binary */
    ObjectFile *reset;

    /** Object pointer for the hypervisor code */
    ObjectFile *hypervisor;

    /** Object pointer for the openboot code */
    ObjectFile *openboot;

    /** Object pointer for the nvram image */
    ObjectFile *nvram;

    /** Object pointer for the hypervisor description image */
    ObjectFile *hypervisor_desc;

    /** Object pointer for the partition description image */
    ObjectFile *partition_desc;

  protected:
    const Params *params() const { return (const Params *)&_params; }

    /** Add a function-based event to reset binary. */
    template <class T>
    T *
    addResetFuncEvent(const char *lbl)
    {
        return addFuncEvent<T>(resetSymtab, lbl);
    }

    /** Add a function-based event to the hypervisor. */
    template <class T>
    T *
    addHypervisorFuncEvent(const char *lbl)
    {
        return addFuncEvent<T>(hypervisorSymtab, lbl);
    }

    /** Add a function-based event to the openboot. */
    template <class T>
    T *
    addOpenbootFuncEvent(const char *lbl)
    {
        return addFuncEvent<T>(openbootSymtab, lbl);
    }
};

} // namespace SparcISA

#endif // __ARCH_SPARC_FS_WORKLOAD_HH__