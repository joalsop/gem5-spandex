/*
 * Copyright (c) 2012-2015 Advanced Micro Devices, Inc.
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

#include "mem/ruby/common/WriteMask.hh"

#include <string>

#include "mem/ruby/common/TypeDefines.hh"
#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

WriteMask::WriteMask()
    : mSize(RubySystem::getBlockSizeBytes()), mMask(mSize, false),
      mAtomic(false)
    {}

WriteMask::WriteMask(int size)
      : mSize(size), mMask(size, false), mAtomic(false)
    {}

WriteMask::WriteMask(int size, std::vector<bool> & mask)
      : mSize(size), mMask(mask), mAtomic(false)
    {}

WriteMask::WriteMask(int size, std::vector<bool> &mask,
              std::vector<std::pair<int, AtomicOpFunctor*> > atomicOp)
      : mSize(size), mMask(mask), mAtomic(true), mAtomicOp(atomicOp)
    {}

int WriteMask::getCount() const
{
    int count = 0;
    for (int i = 0; i < mSize; i += BYTES_PER_WORD) {
        if (mMask.at(i)) {
            count++;
        }
    }
    return count;
}

void
WriteMask::orMask(const WriteMask & writeMask)
{
    assert(mSize == writeMask.mSize);
    for (int i = 0; i < mSize; i++) {
        mMask[i] = (mMask.at(i)) | (writeMask.mMask.at(i));
    }

    if (writeMask.mAtomic) {
        mAtomic = true;
        mAtomicOp = writeMask.mAtomicOp;
    }
}

void
WriteMask::andMask(const WriteMask & writeMask)
{
    assert(mSize == writeMask.mSize);
    for (int i = 0; i < mSize; i++) {
        mMask[i] = (mMask.at(i)) && (writeMask.mMask.at(i));
    }

    if (writeMask.mAtomic) {
        mAtomic = true;
        mAtomicOp = writeMask.mAtomicOp;
    }
}

void
WriteMask::performAtomic(uint8_t * p) const
{
    for (int i = 0; i < mAtomicOp.size(); i++) {
        int offset = mAtomicOp[i].first;
        AtomicOpFunctor *fnctr = mAtomicOp[i].second;
        (*fnctr)(&p[offset]);
    }
}

void
WriteMask::performAtomic(DataBlock & blk) const
{
    for (int i = 0; i < mAtomicOp.size(); i++) {
        int offset = mAtomicOp[i].first;
        uint8_t *p = blk.getDataMod(offset);
        AtomicOpFunctor *fnctr = mAtomicOp[i].second;
        (*fnctr)(p);
    }
}
    bool WriteMask::isFirstValid(Addr address) const {
        int idx = address - makeLineAddress(address);
        for (int i = 0; i < mSize; i++) {
            if (mMask[i]) {
                return (idx == i);
            }
        }
        return false;
    }

    bool WriteMask::isLastValid(Addr address) const {
        int idx = address - makeLineAddress(address);
        for (int i = (mSize-1); i >= 0; i--) {
            if (mMask[i]) {
                return (idx == (i&~3));
            }
        }
        return false;
    }
// Should be static, but that can't be used in slicc
int WriteMask::getWordOffset(Addr physAddr) const {
    return (physAddr-makeLineAddress(physAddr))/BYTES_PER_WORD;
}
int WriteMask::getByteOffset(Addr physAddr) const {
    return (physAddr-makeLineAddress(physAddr));
}

void
WriteMask::print(std::ostream& out) const
{
    std::string str(mSize,'0');
    for (int i = 0; i < mSize; i++) {
        str[i] = mMask[i] ? ('1') : ('0');
    }
    out << "dirty mask="
        << str
        << std::flush;
}

} // namespace ruby
} // namespace gem5
