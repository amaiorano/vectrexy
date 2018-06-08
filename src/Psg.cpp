#include "Psg.h"
#include "BitOps.h"

void Psg::WriteDA(uint8_t value) {
    m_DA = value;
}

uint8_t Psg::ReadDA() {
    return m_DA;
}

void Psg::Reset() {
    m_mode = {};
    m_DA = {};
}

void Psg::Update(cycles_t cycles) {
    for (cycles_t cycle = 0; cycle < cycles; ++cycle) {
        Clock();
    }
}

void Psg::Clock() {
    auto ModeFromBDIRandBC1 = [](bool BDIR, bool BC1) -> Psg::PsgMode {
        uint8_t value{};
        SetBits(value, 1, BDIR);
        SetBits(value, 0, BC1);
        return static_cast<Psg::PsgMode>(value);
    };

    const auto lastMode = m_mode;
    m_mode = ModeFromBDIRandBC1(m_BDIR, m_BC1);

    switch (m_mode) {
    case PsgMode::Inactive:
        break;
    case PsgMode::Read:
        if (lastMode == PsgMode::Inactive) {
            m_DA = m_registers[m_latchedAddress];
        }
        break;
    case PsgMode::Write:
        if (lastMode == PsgMode::Inactive) {
            m_registers[m_latchedAddress] = m_DA;
        }
        break;
    case PsgMode::LatchAddress:
        if (lastMode == PsgMode::Inactive) {
            m_latchedAddress = ReadBits(m_DA, 0b1111);
        }
        break;
    }
}
