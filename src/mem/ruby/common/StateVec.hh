#ifndef STATEVEC_H_
#define STATEVEC_H_

#include <iostream>
#include <vector>

#include "mem/ruby/system/RubySystem.hh"

class StateVec {
    public:
        //Constructor
        StateVec() :
            /* state vector initallized to 0 because we
             * request that the first state enumerated be the default state */
            m_state_vec(RubySystem::getBlockSizeBytes() / BYTES_PER_WORD, 0)
        { }

        //GetSet
        void setAt(int idx, int value);
        int getAt(int idx) const;

        void cpyVec(StateVec);

        bool contains(int state) const;
        bool containsOnly(int state) const;
        void print(std::ostream& out) const;
        //Addr getWordAddr(Addr lineAddr, int idx) const;
        int getSize() const;

    private:
        std::vector<int> m_state_vec;

};

inline std::ostream&
operator<<(std::ostream& out, const StateVec& obj) {
    obj.print(out);
    out << std::flush;
    return out;
}
#endif /* STATEVEC_H_ */
