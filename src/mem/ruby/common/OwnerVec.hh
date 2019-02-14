
#ifndef OWNERVEC_H_
#define OWNERVEC_H_

#include <iostream>
#include <vector>

#include "mem/ruby/common/MachineID.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/common/WriteMask.hh"
#include "mem/ruby/system/RubySystem.hh"

class OwnerVec {
    public:
        //Constructor
        OwnerVec() :
            /* this should use the default initialization of MachineID */
            m_owner_vec(RubySystem::getBlockSizeBytes() / BYTES_PER_WORD)
        { }

        //GetSet
        void        setOwner(int idx, MachineID owner);
        void        setOwner(WriteMask src_mask, MachineID owner);
        void        unsetOwner(WriteMask src_mask, MachineID owner);
        void        unsetOwner(WriteMask src_mask);
        MachineID   getOwner(int idx) const;

        bool        isOwner(MachineID, WriteMask) const;
        bool        isLastOwner(WriteMask) const;

        void cpyVec(const OwnerVec);

        void print(std::ostream& out) const;
        int getSize() const;

        const WriteMask getOwnerMask() const;
        const WriteMask getOwnerMask(MachineID, WriteMask filter) const;
        const NetDest getOwnerNetDest() const;

    private:
        std::vector<MachineID> m_owner_vec;
        WriteMask m_owner_mask;

};

inline std::ostream&
operator<<(std::ostream& out, const OwnerVec& obj) {
    obj.print(out);
    out << std::flush;
    return out;
}
#endif /* OWNERVEC_H_ */
