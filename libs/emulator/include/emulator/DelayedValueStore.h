#pragma once

#include "core/Base.h"

// A simple value container on which we can assign values, but that value will only be returned
// after an input number of cycles.

template <typename T>
class DelayedValueStore {
public:
    cycles_t CyclesToUpdateValue = 0;

    DelayedValueStore& operator=(const T& nextValue) {
        m_nextValue = nextValue;
        m_cyclesLeft = CyclesToUpdateValue;
        if (m_cyclesLeft == 0)
            m_value = m_nextValue;
        return *this;
    }

    void Update(cycles_t cycles) {
        (void)cycles;
        assert(cycles == 1);
        if (m_cyclesLeft > 0 && --m_cyclesLeft == 0) {
            m_value = m_nextValue;
        }
    }

    const T& Value() const { return m_value; }
    operator const T&() const { return Value(); }

private:
    cycles_t m_cyclesLeft{};
    T m_nextValue{};
    T m_value{};
};
