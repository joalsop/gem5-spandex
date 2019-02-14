#include "mem/ruby/common/OwnerVec.hh"

void
OwnerVec::setOwner(int idx, MachineID owner) {
    m_owner_vec[idx] = owner;
    m_owner_mask.setMask(idx, BYTES_PER_WORD);
}

void
OwnerVec::setOwner(const WriteMask in_mask, MachineID owner) {
    assert(m_owner_mask.getSize() == in_mask.getSize());

    m_owner_mask.orMask(in_mask);
    for (int i=0; i<m_owner_mask.getSize(); i += BYTES_PER_WORD) {
        if (in_mask.getMask(i, BYTES_PER_WORD)) {
            int word_idx = i/BYTES_PER_WORD;
            m_owner_vec[word_idx] = owner;
        }
    }
}

/* Takes a write mask and owner id to test, if the owner listed in @this
 * OwnerVec matches the owner under test, @this OwnerVec deregisters the
 * owner. */
void
OwnerVec::unsetOwner(const WriteMask in_mask, MachineID owner) {
    assert(m_owner_mask.getSize() == in_mask.getSize());

    for (int i=0; i<m_owner_mask.getSize(); i += BYTES_PER_WORD) {
        int word_idx = i/BYTES_PER_WORD;
        if (    (m_owner_vec[word_idx] == owner) &&
                (in_mask.getMask(i, BYTES_PER_WORD)) ) {
            m_owner_mask.unsetMask(i, BYTES_PER_WORD);
            MachineID default_mid;
            m_owner_vec[word_idx] = default_mid;
        }
    }
}

/* Takes a write mask and registers owner for those words in @this
 * Does not check who is current owner.
 * */
void
OwnerVec::unsetOwner(const WriteMask in_mask) {
    assert(m_owner_mask.getSize() == in_mask.getSize());

    for (int i=0; i<m_owner_mask.getSize(); i += BYTES_PER_WORD) {
        int word_idx = i/BYTES_PER_WORD;
        if ( (in_mask.getMask(i, BYTES_PER_WORD)) ) {
            m_owner_mask.unsetMask(i, BYTES_PER_WORD);
            MachineID default_mid;
            m_owner_vec[word_idx] = default_mid;
        }
    }
}

MachineID
OwnerVec::getOwner(int idx) const {
    return m_owner_vec[idx];
}


void
OwnerVec::cpyVec(const OwnerVec src) {
    assert(m_owner_vec.size() == src.getSize());
    for (int i = 0; i < m_owner_vec.size(); i++) {
        m_owner_vec[i] = src.getOwner(i);
    }
    m_owner_mask.cpyMask(src.getOwnerMask());
}

int
OwnerVec::getSize() const {
    return m_owner_vec.size();
}

const WriteMask
OwnerVec::getOwnerMask() const {
    return m_owner_mask;
}

const WriteMask
OwnerVec::getOwnerMask(MachineID core, WriteMask filter) const {
    WriteMask retval;
    for (int i = 0; i < m_owner_vec.size(); i++) {
        if (filter.getMask(i*BYTES_PER_WORD, BYTES_PER_WORD) &&
                (core == m_owner_vec[i])) {

            retval.setMask(i*BYTES_PER_WORD, BYTES_PER_WORD);
        }
    }
    return retval;
}

const NetDest
OwnerVec::getOwnerNetDest() const {
    NetDest retval;
    int idx = 0;
    for (int i=0; i<m_owner_mask.getSize(); i += BYTES_PER_WORD) {
        if (m_owner_mask.getMask(i, BYTES_PER_WORD)) {
            retval.add(m_owner_vec[idx]);
        }
        idx++;
    }
    return retval;
}

bool
OwnerVec::isOwner(MachineID core, WriteMask check_mask) const {
    for (int i = 0; i < m_owner_vec.size(); i++) {
        if (m_owner_mask.getMask(i*BYTES_PER_WORD, BYTES_PER_WORD) &&
                (core != m_owner_vec[i])) {
            return false;
        }
    }
    return true;
}

bool
OwnerVec::isLastOwner(WriteMask check_mask) const {
    WriteMask tmp_mask;
    tmp_mask.cpyMask(m_owner_mask);
    tmp_mask.unsetMask(check_mask);
    return (tmp_mask.isEmpty());
}

void
OwnerVec::print(std::ostream& out) const {
    out << "[ ";
    for (MachineID o : m_owner_vec) {
        out << o;
    }
    out << "]";
    out << std::flush;
}
