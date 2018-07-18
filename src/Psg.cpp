#include "Psg.h"
#include "BitOps.h"
#include "EngineClient.h"
#include "Gui.h"
#include <cmath>

namespace {
    namespace Register {
        enum Type {
            //@TODO rename "Channel" to "ToneGeneratorControl" or something
            ToneGeneratorALow = 0,
            ToneGeneratorAHigh = 1,
            ToneGeneratorBLow = 2,
            ToneGeneratorBHigh = 3,
            ToneGeneratorCLow = 4,
            ToneGeneratorCHigh = 5,
            NoiseGenerator = 6,
            MixerControl = 7,
            AmplitudeA = 8,
            AmplitudeB = 9,
            AmplitudeC = 10,
            EnvelopePeriodLow = 11,
            EnvelopePeriodHigh = 12,
            EnvelopeShape = 13,
            IOPortADataStore = 14,
            IOPortBDataStore = 15
        };
    }

    namespace MixerControlRegister {
        const uint8_t ToneA = BITS(0);
        const uint8_t ToneB = BITS(1);
        const uint8_t ToneC = BITS(2);
        const uint8_t NoiseA = BITS(3);
        const uint8_t NoiseB = BITS(4);
        const uint8_t NoiseC = BITS(5);
        // Bits 6 and 7 are to control IO ports A and B, but we don't use this on Vectrex

        bool IsEnabled(uint8_t reg, uint8_t type) {
            // Enabled when bit is 0
            return !TestBits(reg, type);
        }
    } // namespace MixerControlRegister

    namespace AmplitudeControlRegister {
        const uint8_t FixedVolume = BITS(0, 1, 2, 3);
        const uint8_t EnvelopeMode = BITS(4);
        const uint8_t Unused = BITS(5, 6, 7);

        AmplitudeMode GetMode(uint8_t reg) {
            return TestBits(reg, EnvelopeMode) ? AmplitudeMode::Envelope : AmplitudeMode::Fixed;
        }

        uint32_t GetFixedVolume(uint8_t reg) {
            // assert(GetMode(reg) == Mode::Fixed);
            return ReadBits(reg, FixedVolume);
        }

    } // namespace AmplitudeControlRegister
} // namespace

Psg::Psg()
    : m_channels{PsgChannel{m_toneGenerators[0], m_noiseGenerator, m_envelopeGenerator},
                 PsgChannel{m_toneGenerators[1], m_noiseGenerator, m_envelopeGenerator},
                 PsgChannel{m_toneGenerators[2], m_noiseGenerator, m_envelopeGenerator}} {}

void Psg::Init() {
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
    m_masterDivider.Reset();
    m_toneGenerators = {};
    m_noiseGenerator = {};
    m_envelopeGenerator = {};
}

void Psg::Update(cycles_t cycles) {
    for (cycles_t cycle = 0; cycle < cycles; ++cycle) {
        Clock();
    }
}

void Psg::FrameUpdate() {
    // Debug output
    static bool PsgImGui = false;
    IMGUI_CALL(Debug, ImGui::Checkbox("<<< Psg >>>", &PsgImGui));
    if (PsgImGui) {
        static int index = 0;
        static std::array<float, 10000> envelopeHistory;

        envelopeHistory[index] = (float)m_envelopeGenerator.Value();
        if (index == 0) {
            std::fill(envelopeHistory.begin(), envelopeHistory.end(), 0.f);
        }
        index = (index + 1) % envelopeHistory.size();
        IMGUI_CALL(Debug,
                   ImGui::PlotLines("Envelope", envelopeHistory.data(), (int)envelopeHistory.size(),
                                    0, 0, 0.f, 15.f, ImVec2(0, 100.f)));
    }
}

void Psg::Clock() {
    auto ModeFromBDIRandBC1 = [](bool BDIR, bool BC1) -> Psg::PsgMode {
        uint8_t value{};
        SetBits(value, 0b10, BDIR);
        SetBits(value, 0b01, BC1);
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

    // Clock generators every 16 input clocks
    if (m_masterDivider.Clock()) {
        for (auto& toneGenerator : m_toneGenerators) {
            toneGenerator.Clock();
        }
        m_noiseGenerator.Clock();
        m_envelopeGenerator.Clock();
    }
}

bool Psg::IsProducingSound() const {
    for (auto& channel : m_channels) {
        if (channel.ToneEnabled() || channel.NoiseEnabled())
            return true;
    }
    return false;
}

float Psg::Sample() const {
    // Sample and mix each of the 3 channels
    float sample = 0.f;
    for (auto& channel : m_channels) {
        sample += channel.Sample();
    }
    sample /= 3.f;
    return sample;
}

uint8_t Psg::Read(uint16_t address) {
    switch (m_latchedAddress) {
    case Register::ToneGeneratorALow:
        return m_toneGenerators[0].PeriodLow();
    case Register::ToneGeneratorAHigh:
        return m_toneGenerators[0].PeriodHigh();
    case Register::ToneGeneratorBLow:
        return m_toneGenerators[1].PeriodLow();
    case Register::ToneGeneratorBHigh:
        return m_toneGenerators[1].PeriodHigh();
    case Register::ToneGeneratorCLow:
        return m_toneGenerators[2].PeriodLow();
    case Register::ToneGeneratorCHigh:
        return m_toneGenerators[2].PeriodHigh();
    case Register::NoiseGenerator:
        return m_noiseGenerator.Period();
    case Register::MixerControl:
        break;
    case Register::AmplitudeA:
    case Register::AmplitudeB:
    case Register::AmplitudeC:
        break;
    case Register::EnvelopePeriodLow:
        return m_envelopeGenerator.PeriodLow();
    case Register::EnvelopePeriodHigh:
        return m_envelopeGenerator.PeriodHigh();
    case Register::EnvelopeShape:
        return m_envelopeGenerator.Shape();
    case Register::IOPortADataStore:
    case Register::IOPortBDataStore:
        break;
    default:
        ASSERT(false);
    }

    return m_registers[address];
}

void Psg::Write(uint16_t address, uint8_t value) {
    switch (m_latchedAddress) {
    case Register::ToneGeneratorALow:
        return m_toneGenerators[0].SetPeriodLow(value);
    case Register::ToneGeneratorAHigh:
        return m_toneGenerators[0].SetPeriodHigh(value);
    case Register::ToneGeneratorBLow:
        return m_toneGenerators[1].SetPeriodLow(value);
    case Register::ToneGeneratorBHigh:
        return m_toneGenerators[1].SetPeriodHigh(value);
    case Register::ToneGeneratorCLow:
        return m_toneGenerators[2].SetPeriodLow(value);
    case Register::ToneGeneratorCHigh:
        return m_toneGenerators[2].SetPeriodHigh(value);
    case Register::NoiseGenerator:
        return m_noiseGenerator.SetPeriod(value);
    case Register::MixerControl:
        ASSERT_MSG(ReadBits(value, 0b1100'0000) == 0, "Not supporting I/O ports on PSG");
        m_channels[0].SetToneEnabled(
            MixerControlRegister::IsEnabled(value, MixerControlRegister::ToneA));
        m_channels[1].SetToneEnabled(
            MixerControlRegister::IsEnabled(value, MixerControlRegister::ToneB));
        m_channels[2].SetToneEnabled(
            MixerControlRegister::IsEnabled(value, MixerControlRegister::ToneC));
        m_channels[0].SetNoiseEnabled(
            MixerControlRegister::IsEnabled(value, MixerControlRegister::NoiseA));
        m_channels[1].SetNoiseEnabled(
            MixerControlRegister::IsEnabled(value, MixerControlRegister::NoiseB));
        m_channels[2].SetNoiseEnabled(
            MixerControlRegister::IsEnabled(value, MixerControlRegister::NoiseC));
        break;
    case Register::AmplitudeA:
    case Register::AmplitudeB:
    case Register::AmplitudeC: {
        auto& channel = m_channels[m_latchedAddress - Register::AmplitudeA];
        channel.GetAmplitudeControl().SetMode(AmplitudeControlRegister::GetMode(value));
        channel.GetAmplitudeControl().SetFixedVolume(
            AmplitudeControlRegister::GetFixedVolume(value));
    } break;
    case Register::EnvelopePeriodLow:
        return m_envelopeGenerator.SetPeriodLow(value);
    case Register::EnvelopePeriodHigh:
        return m_envelopeGenerator.SetPeriodHigh(value);
    case Register::EnvelopeShape:
        return m_envelopeGenerator.SetShape(value);
    case Register::IOPortADataStore:
    case Register::IOPortBDataStore:
        break;
    default:
        ASSERT(false);
    }

    m_registers[address] = value;
}
