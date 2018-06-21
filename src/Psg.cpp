#include "Psg.h"
#include "BitOps.h"

namespace {
    namespace Register {
        enum Type {
            ChannelALow = 0,
            ChannelAHigh = 1,
            ChannelBLow = 2,
            ChannelBHigh = 3,
            ChannelCLow = 4,
            ChannelCHigh = 5,
        };
    }
} // namespace

struct SquareWaveGenerator {
    uint32_t m_period{}; // 12 bit value [0,4095]
    uint32_t m_duty{};   // Period / 2 (number of clocks before oscilating)
    uint32_t m_time{};   // Time in period
    uint16_t m_value{};  // 0 or 1

    const uint32_t PeriodScale = 16; // Psg hardware always counts min 16

    void SetPeriodHigh(uint8_t high) {
        assert(high <= 0xf); // Only 4 bits should be set
        m_period |= high << 16;
        UpdateDuty();
    }
    void SetPeriodLow(uint8_t low) {
        m_period |= low;
        UpdateDuty();
    }

    uint8_t PeriodHigh() const {
        return checked_static_cast<uint8_t>(m_period >> 16); // Top 12 bits
    }
    uint8_t PeriodLow() const { return checked_static_cast<uint8_t>(m_period & 0xff); }

    void Clock() {
        if (--m_time == 0) {
            m_value = m_value == 0 ? 1 : 0;
            m_time = m_duty;
        }
    }

    uint16_t Value() const { m_value; }

private:
    void UpdateDuty() {
        m_duty = (std::max<uint32_t>(1, m_period) * PeriodScale) / 2;
        assert(m_duty > 0);
        // Should we reset time?
    }
};

void Psg::Init() {
    for (auto& swg : m_squareWaveGenerators) {
        swg.reset(new SquareWaveGenerator{});
    }

    Reset();
}

void Psg::WriteDA(uint8_t value) {
    m_DA = value;
}

uint8_t Psg::ReadDA() {
    return m_DA;
}

void Psg::Reset() {
    m_mode = {};
    m_DA = {};
    m_registers.fill(0);
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
            m_DA = Read(m_latchedAddress);
        }
        break;
    case PsgMode::Write:
        if (lastMode == PsgMode::Inactive) {
            Write(m_latchedAddress, m_DA);
        }
        break;
    case PsgMode::LatchAddress:
        if (lastMode == PsgMode::Inactive) {
            m_latchedAddress = ReadBits(m_DA, 0b1111);
        }
        break;
    }

    for (auto& swg : m_squareWaveGenerators) {
        swg->Clock();
    }
}

uint8_t Psg::Read(uint16_t address) {
    switch (m_latchedAddress) {
    case Register::ChannelAHigh:
        return m_squareWaveGenerators[0]->PeriodHigh();
    case Register::ChannelALow:
        return m_squareWaveGenerators[0]->PeriodLow();
    case Register::ChannelBHigh:
        return m_squareWaveGenerators[1]->PeriodHigh();
    case Register::ChannelBLow:
        return m_squareWaveGenerators[1]->PeriodLow();
    case Register::ChannelCHigh:
        return m_squareWaveGenerators[2]->PeriodHigh();
    case Register::ChannelCLow:
        return m_squareWaveGenerators[2]->PeriodLow();
    }

    return m_registers[address];
}

void Psg::Write(uint16_t address, uint8_t value) {
    switch (m_latchedAddress) {
    case Register::ChannelAHigh:
        return m_squareWaveGenerators[0]->SetPeriodHigh(value);
    case Register::ChannelALow:
        return m_squareWaveGenerators[0]->SetPeriodLow(value);
    case Register::ChannelBHigh:
        return m_squareWaveGenerators[1]->SetPeriodHigh(value);
    case Register::ChannelBLow:
        return m_squareWaveGenerators[1]->SetPeriodLow(value);
    case Register::ChannelCHigh:
        return m_squareWaveGenerators[2]->SetPeriodHigh(value);
    case Register::ChannelCLow:
        return m_squareWaveGenerators[2]->SetPeriodLow(value);
    }

    m_registers[address] = value;
}
