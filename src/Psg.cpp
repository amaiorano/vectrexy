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

    namespace MixerControl {
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

        uint8_t ToneChannelByIndex(int index) { return ToneA << index; }
        uint8_t NoiseChannelByIndex(int index) { return NoiseA << index; }

    } // namespace MixerControl

    namespace AmplitudeControl {
        const uint8_t FixedVolume = BITS(0, 1, 2, 3);
        const uint8_t EnvelopeMode = BITS(4);
        const uint8_t Unused = BITS(5, 6, 7);

        enum class Mode { Fixed, Envelope };
        Mode GetMode(uint8_t reg) {
            return TestBits(reg, EnvelopeMode) ? Mode::Envelope : Mode::Fixed;
        }

        uint32_t GetFixedVolume(uint8_t reg) {
            assert(GetMode(reg) == Mode::Fixed);
            return ReadBits(reg, FixedVolume);
        }

    } // namespace AmplitudeControl
} // namespace

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
}

void Psg::Update(cycles_t cycles) {
    for (cycles_t cycle = 0; cycle < cycles; ++cycle) {
        Clock();
    }
}

static bool g_multiSample = false;

void Psg::FrameUpdate() {
    // Debug output
    {
        static int index = 0;
        static std::array<float, 10000> envelopeHistory;

        envelopeHistory[index] = (float)m_envelopeGenerator.Value();
        if (index == 0) {
            std::fill(envelopeHistory.begin(), envelopeHistory.end(), 0.f);
        }
        index = (index + 1) % envelopeHistory.size();
        IMGUI_CALL(AudioDebug,
                   ImGui::PlotLines("Envelope", envelopeHistory.data(), (int)envelopeHistory.size(),
                                    0, 0, 0.f, 15.f, ImVec2(0, 100.f)));

        IMGUI_CALL(AudioDebug, ImGui::Checkbox("Multisample", &g_multiSample));
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

    m_sampleSum += SampleChannelsAndMix();
    ++m_numSamples;
}

bool Psg::IsProducingSound() const {
    for (int i = 0; i < 3; ++i) {
        auto& mixerControlRegister = m_registers[Register::MixerControl];
        auto toneChannel = MixerControl::ToneChannelByIndex(i);
        auto noiseChannel = MixerControl::NoiseChannelByIndex(i);
        bool toneEnabled = MixerControl::IsEnabled(mixerControlRegister, toneChannel);
        bool noiseEnabled = MixerControl::IsEnabled(mixerControlRegister, noiseChannel);
        if (toneEnabled || noiseEnabled)
            return true;
    }
    return false;
}

float Psg::Sample() const {
    if (g_multiSample) {
        float outputSample = m_sampleSum / m_numSamples;
        //@TODO: No mutable state here!
        m_sampleSum = m_numSamples = 0;
        return outputSample;
    } else {
        return SampleChannelsAndMix();
    }
}

float Psg::SampleChannelsAndMix() const {
    auto GetChannelVolume = [](const uint8_t& amplitudeRegister,
                               const EnvelopeGenerator& envelopeGenerator) {
        uint32_t volume = 0;
        switch (AmplitudeControl::GetMode(amplitudeRegister)) {
        case AmplitudeControl::Mode::Fixed:
            volume = AmplitudeControl::GetFixedVolume(amplitudeRegister);
            break;
        case AmplitudeControl::Mode::Envelope:
            volume = envelopeGenerator.Value();
            break;
        }

        // The volume is non-linear. Below formula does comply with the PSG datasheet, and does more
        // or less match the voltages measured on the CPCs speaker (the voltages on the CPCs stereo
        // connector seem to be slightly different though).
        // amplitude = max / sqrt(2)^(15-nn)
        // eg. 15 --> max / 1, 14 --> max / 1.414, 13 --> max / 2, etc.
        // http://www.cpcwiki.eu/index.php/PSG#0Ah_-_Channel_C_Volume_.280-0Fh.3Dvolume.2C_10h.3Duse_envelope_instead.29
        assert(volume < 16);
        if (volume == 0)
            return 0.f;
        return 1.f / ::powf(::sqrtf(2), 15.f - volume);
    };

    auto SampleChannel = [&GetChannelVolume](const uint8_t& amplitudeRegister,
                                             const uint8_t& mixerControlRegister, int index,
                                             const ToneGenerator& toneGenerator,
                                             const NoiseGenerator& noiseGenerator,
                                             const EnvelopeGenerator& envelopeGenerator) -> float {
        const float volume = GetChannelVolume(amplitudeRegister, envelopeGenerator);
        if (volume == 0.f)
            return 0.5f;

        // If both Tone and Noise are disabled on a channel, then a constant HIGH level is output
        // (useful for digitized speech). If both Tone and Noise are enabled on the same channel,
        // then the signals are ANDed (the signals aren't ADDed) (ie. HIGH is output only if both
        // are HIGH).
        // http://www.cpcwiki.eu/index.php/PSG#07h_-_Mixer_Control_Register

        auto toneChannel = MixerControl::ToneChannelByIndex(index);
        auto noiseChannel = MixerControl::NoiseChannelByIndex(index);
        bool toneEnabled = MixerControl::IsEnabled(mixerControlRegister, toneChannel);
        bool noiseEnabled = MixerControl::IsEnabled(mixerControlRegister, noiseChannel);

        uint32_t sample = 0;
        if (toneEnabled && noiseEnabled) {
            sample = toneGenerator.Value() & noiseGenerator.Value();
        } else if (toneEnabled) {
            sample = toneGenerator.Value();
        } else if (noiseEnabled) {
            sample = noiseGenerator.Value();
        }

        return ((sample - 0.5f) * volume) + 0.5f;
    };

    // Sample and mix each of the 3 channels
    float sample = 0.f;
    for (int i = 0; i < 3; ++i) {
        auto& amplitudeRegister = m_registers[Register::AmplitudeA + i];
        auto& mixerControlRegister = m_registers[Register::MixerControl];
        sample += SampleChannel(amplitudeRegister, mixerControlRegister, i, m_toneGenerators[i],
                                m_noiseGenerator, m_envelopeGenerator);
    }
    sample /= 3.f;
    return sample;
}

uint8_t Psg::Read(uint16_t address) {
    switch (m_latchedAddress) {
    case Register::ToneGeneratorAHigh:
        return m_toneGenerators[0].PeriodHigh();
    case Register::ToneGeneratorALow:
        return m_toneGenerators[0].PeriodLow();
    case Register::ToneGeneratorBHigh:
        return m_toneGenerators[1].PeriodHigh();
    case Register::ToneGeneratorBLow:
        return m_toneGenerators[1].PeriodLow();
    case Register::ToneGeneratorCHigh:
        return m_toneGenerators[2].PeriodHigh();
    case Register::ToneGeneratorCLow:
        return m_toneGenerators[2].PeriodLow();
    case Register::NoiseGenerator:
        return m_noiseGenerator.Period();
    case Register::EnvelopePeriodHigh:
        return m_envelopeGenerator.PeriodHigh();
    case Register::EnvelopePeriodLow:
        return m_envelopeGenerator.PeriodLow();
    case Register::EnvelopeShape:
        return m_envelopeGenerator.Shape();
    }

    return m_registers[address];
}

void Psg::Write(uint16_t address, uint8_t value) {
    switch (m_latchedAddress) {
    case Register::ToneGeneratorAHigh:
        return m_toneGenerators[0].SetPeriodHigh(value);
    case Register::ToneGeneratorALow:
        return m_toneGenerators[0].SetPeriodLow(value);
    case Register::ToneGeneratorBHigh:
        return m_toneGenerators[1].SetPeriodHigh(value);
    case Register::ToneGeneratorBLow:
        return m_toneGenerators[1].SetPeriodLow(value);
    case Register::ToneGeneratorCHigh:
        return m_toneGenerators[2].SetPeriodHigh(value);
    case Register::ToneGeneratorCLow:
        return m_toneGenerators[2].SetPeriodLow(value);
    case Register::NoiseGenerator:
        return m_noiseGenerator.SetPeriod(value);
    case Register::MixerControl:
        ASSERT_MSG(ReadBits(value, 0b1100'0000) == 0, "Not supporting I/O ports on PSG");
        break;
    case Register::EnvelopePeriodHigh:
        return m_envelopeGenerator.SetPeriodHigh(value);
    case Register::EnvelopePeriodLow:
        return m_envelopeGenerator.SetPeriodLow(value);
    case Register::EnvelopeShape:
        return m_envelopeGenerator.SetShape(value);
    }

    m_registers[address] = value;
}
