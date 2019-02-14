#include "mem/ruby/common/StateVec.hh"

void
StateVec::setAt(int idx, int state) {
    m_state_vec[idx] = state;
}

int
StateVec::getAt(int idx) const {
    return m_state_vec[idx];
}

bool
StateVec::contains(int state) const {
    for (int s : m_state_vec) {
        if (s == state) {
            return true;
        }
    }
    return false;
}

bool
StateVec::containsOnly(int state) const {
    for (int s : m_state_vec) {
        if (s != state) {
            return false;
        }
    }
    return true;
}

void
StateVec::cpyVec(StateVec src) {
    assert(m_state_vec.size() == src.getSize());
    for (int i = 0; i < m_state_vec.size(); i++) {
        m_state_vec[i] = src.getAt(i);
    }
}
//Addr
//StateVec::getWordAddr(Addr lineAddr, int idx) const {
//    Addr retval = lineAddr + BYTES_PER_WORD*idx;
//    return retval;
//}

int
StateVec::getSize() const {
    return m_state_vec.size();
}

void
StateVec::print(std::ostream& out) const {
    out << "[ ";
    for (int b : m_state_vec) {
        out << b;
    }
    out << std::flush;
}
