#include "emulator/Psg.h"
#include "core/BitOps.h"
#include "core/ErrorHandler.h"
#include "core/Gui.h"
#include "emulator/EngineTypes.h"
#include <array>
#include <cmath>
#include <memory>

namespace {
    enum class AmplitudeMode { Fixed, Envelope };

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

        uint32_t GetFixedVolume(uint8_t reg) { return ReadBits(reg, FixedVolume); }

    } // namespace AmplitudeControlRegister

    //@TODO: move into utility header
    template <typename T, size_t size>
    class PlotData {
    public:
        using ArrayType = std::array<T, size>;

        PlotData() { Clear(); }

        void Clear() { std::fill(values.begin(), values.end(), 0.f); }

        void AddValue(const T& value) {
            if (index == 0)
                Clear();
            values[index] = value;
            index = (index + 1) % values.size();
        }

        const ArrayType& Values() const { return values; }

    private:
        ArrayType values;
        size_t index = 0;
    };

    // Timer used by Tone and Noise Generators
    class Timer {
    public:
        Timer(uint32_t period = 0) {
            SetPeriod(period);
            Reset();
        }

        // Resets time
        void SetPeriod(uint32_t period) {
            // Keep relative position when changing period
            float ratio = m_period == 0 ? 0.f : static_cast<float>(m_time) / m_period;
            m_period = period;
            m_time = static_cast<uint32_t>(m_period * ratio);
        }
        uint32_t Period() const { return m_period; }

        void Reset() { m_time = 0; }

        // Returns true when timer expires (and auto-resets)
        bool Clock() {
            if ((m_period > 0) && (++m_time == m_period)) {
                Reset();
                return true;
            }
            return false;
        }

    private:
        uint32_t m_period{};
        uint32_t m_time{}; // Time in period
    };

    class ToneGenerator {
    public:
        void SetPeriodHigh(uint8_t high) {
            assert(high <= 0xff); // Only 8 bits should be set
            m_period = (high << 8) | (m_period & 0x00ff);
            OnPeriodUpdated();
        }
        void SetPeriodLow(uint8_t low) {
            m_period = (m_period & 0xff00) | low;
            OnPeriodUpdated();
        }

        uint8_t PeriodHigh() const {
            return checked_static_cast<uint8_t>(m_period >> 8); // Top 4 bits
        }
        uint8_t PeriodLow() const { return checked_static_cast<uint8_t>(m_period & 0xff); }

        // When period is 0, we don't want to hear anything from the tone generator
        bool IsEnabled() const { return m_period > 0; }

        void Clock() {
            if (m_timer.Clock()) {
                m_value = (m_value == 0 ? 1 : 0);
            }
        }

        uint32_t Value() const { return m_value; }

    private:
        void OnPeriodUpdated() {
            // Note: changing period does not reset value
            auto duty = std::max<uint32_t>(1, m_period / 2);
            m_timer.SetPeriod(duty);
        }

        Timer m_timer;
        uint32_t m_period{}; // 12 bit value [0,4095]
        uint32_t m_value{};  // 0 or 1
    };

    class NoiseGenerator {
    public:
        void SetPeriod(uint8_t period) {
            assert(period < 32);
            m_period = period;
            OnPeriodUpdated();
        }

        uint8_t Period() const { return static_cast<uint8_t>(m_timer.Period()); }

        // Looks like even when period is 0, noise generator needs to keep generating values
        bool IsEnabled() const { return true; }

        void Clock() {
            if (m_timer.Clock()) {
                ClockShiftRegister();
            }
        }

        uint32_t Value() const { return m_value; }

    private:
        void ClockShiftRegister() {
            // From http://www.cpcwiki.eu/index.php/PSG#06h_-_Noise_Frequency_.285bit.29
            // noise_level = noise_level XOR shiftreg.bit0
            // newbit = shiftreg.bit0 XOR shiftreg.bit3
            // shiftreg = (shiftreg SHR 1) + (newbit SHL 16)
            uint32_t bit0 = ReadBits(m_shiftRegister, BITS(0));
            uint32_t bit3 = ReadBits(m_shiftRegister, BITS(3)) >> 3;
            m_value = m_value ^ bit0;
            uint32_t newBit = bit0 ^ bit3;
            m_shiftRegister = (m_shiftRegister >> 1) | (newBit << 16);
            ASSERT(m_shiftRegister < BITS(18));
        }

        void OnPeriodUpdated() { m_timer.SetPeriod(std::max<uint8_t>(1, m_period & 0b0001'1111)); }

        Timer m_timer;
        uint32_t m_period{};          // 5 bit value [0,32]
        uint32_t m_shiftRegister = 1; // Must be initialized to a non-zero value
        uint32_t m_value{};           // 0 or 1
    };

    class EnvelopeGenerator {
    public:
        void SetPeriodHigh(uint8_t high) {
            m_period = (high << 8) | (m_period & 0x00ff);
            OnPeriodUpdated();
        }
        void SetPeriodLow(uint8_t low) {
            m_period = (m_period & 0xff00) | low;
            OnPeriodUpdated();
        }

        void SetShape(uint8_t shape) {
            assert(shape < 16);
            m_shape = shape;
            m_currShapeIndex = 0;
            UpdateValue();
        }

        uint8_t PeriodHigh() const { return checked_static_cast<uint8_t>(m_period >> 8); }

        uint8_t PeriodLow() const { return checked_static_cast<uint8_t>(m_period & 0xff); }

        uint8_t Shape() { return m_shape; }

        void Clock() {
            if (m_divider.Clock() && m_timer.Clock()) {
                UpdateValue();
            }
        }

        uint32_t Value() const { return m_value; }

    private:
        void OnPeriodUpdated() {
            //@TODO: why am I dividing by 16 here?
            auto timeToIncrementShapeIndex = std::max<uint32_t>(1, m_period / 16);
            m_timer.SetPeriod(timeToIncrementShapeIndex);
            UpdateValue();
        }

        void UpdateValue() {
            using Shape = std::array<uint32_t, 32>;
            using Table = std::array<Shape, 16>;

            // 4-bit pattern where bits:
            // Bit 3: continue
            // Bit 2: attack
            // Bit 1: alternate
            // Bit 0: hold
            // Instead of a state machine, we use a lookup table.
            // This table contains 2 cycles worth of values.
            static const Table table = {
                // clang-format off
                // 0000 to 0011
                Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                // 0100 to 0111
                Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                // 1000
                Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },
                // 1001
                Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                // 1010
                Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
                // 1011
                Shape{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },
                // 1100
                Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
                // 1101
                Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },
                // 1110
                Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },
                // 1111
                Shape{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
                // clang-format on
            };

            const Shape& shape = table[m_shape];

            m_value = shape[m_currShapeIndex];

            // If we're on the last value, then check if we should hold this last value or restart
            // the shape.
            const bool holdCurrentIndex = [this, &shape] {
                if (m_currShapeIndex < (shape.size() - 1))
                    return false;

                bool continuePattern = TestBits(m_shape, 0b1000);
                if (!continuePattern)
                    return true;

                bool holdPattern = TestBits(m_shape, 0b0001);
                return holdPattern;
            }();

            if (!holdCurrentIndex) {
                m_currShapeIndex = (m_currShapeIndex + 1) % shape.size();
            }
        }

        Timer m_divider{
            16}; // Envelope further divides by 16 (so it produces values every 256 cycles)
        Timer m_timer;
        uint32_t m_period{}; // 16 bit value
        uint32_t m_value{};  // 0 or 1

        uint8_t m_shape{};          // 4 bit shape index
        uint8_t m_currShapeIndex{}; // Index into current shape (0-31)
    };

    class AmplitudeControl {
    public:
        AmplitudeControl(EnvelopeGenerator& envelopeGenerator)
            : m_envelopeGenerator(envelopeGenerator) {}

        void SetMode(AmplitudeMode mode) { m_mode = mode; }
        void SetFixedVolume(uint32_t volume) { m_fixedVolume = volume; }

        // Returns volume in [0,1]
        float Volume() const {
            uint32_t volume = 0;
            switch (m_mode) {
            case AmplitudeMode::Fixed:
                volume = m_fixedVolume;
                break;
            case AmplitudeMode::Envelope:
                volume = m_envelopeGenerator.Value();
                break;
            }

            assert(volume < 16);

            // There's a bug in the Vectrex BIOS Clear_Sound routine ($F272) that is suppposed to
            // initialize the PSG registers to 0, but instead, only does so for one register, and
            // sets the rest to 1. This makes it so that we hear some noise when we reset. We can
            // "fix" this here by considering volume 1 to be silent.
            if (volume <= 1)
                return 0.f;

            // The volume is non-linear. Below formula does comply with the PSG datasheet, and does
            // more or less match the voltages measured on the CPCs speaker (the voltages on the
            // CPCs stereo connector seem to be slightly different though). amplitude = max /
            // sqrt(2)^(15-nn) eg. 15 --> max / 1, 14 --> max / 1.414, 13 --> max / 2, etc.
            // http://www.cpcwiki.eu/index.php/PSG#0Ah_-_Channel_C_Volume_.280-0Fh.3Dvolume.2C_10h.3Duse_envelope_instead.29
            return 1.f / ::powf(::sqrtf(2), 15.f - volume);
        }

    private:
        AmplitudeMode m_mode = AmplitudeMode::Fixed;
        uint32_t m_fixedVolume{};
        EnvelopeGenerator& m_envelopeGenerator;
    };

    class PsgChannel {
    public:
        PsgChannel(ToneGenerator& toneGenerator, NoiseGenerator& noiseGenerator,
                   EnvelopeGenerator& envelopeGenerator)
            : m_toneGenerator(toneGenerator)
            , m_noiseGenerator(noiseGenerator)
            , m_amplitudeControl(envelopeGenerator) {}

        bool ToneEnabled() const { return m_toneEnabled; }
        bool NoiseEnabled() const { return m_noiseEnabled; }
        void SetToneEnabled(bool enabled) { m_toneEnabled = enabled; }
        void SetNoiseEnabled(bool enabled) { m_noiseEnabled = enabled; }
        AmplitudeControl& GetAmplitudeControl() { return m_amplitudeControl; }
        const ToneGenerator& GetToneGenerator() const { return m_toneGenerator; }
        const NoiseGenerator& GetNoiseGenerator() const { return m_noiseGenerator; }

        bool OverrideToneEnabled = true;
        bool OverrideNoiseEnabled = true;

        float Sample() const {
            float volume = m_amplitudeControl.Volume();

            // If both Tone and Noise are disabled on a channel, then a constant HIGH level is
            // output (useful for digitized speech). If both Tone and Noise are enabled on the same
            // channel, then the signals are ANDed (the signals aren't ADDed) (ie. HIGH is output
            // only if both are HIGH).
            // http://www.cpcwiki.eu/index.php/PSG#07h_-_Mixer_Control_Register

            uint32_t sample = 0;
            const bool toneEnabled = m_toneEnabled                  // Enabled on channel
                                     && m_toneGenerator.IsEnabled() // Enabled on chip
                                     && OverrideToneEnabled;        // Enabled for debugging

            const bool noiseEnabled = m_noiseEnabled                  // Enabled on channel
                                      && m_noiseGenerator.IsEnabled() // Enabled on chip
                                      && OverrideNoiseEnabled;        // Enabled for debugging

            if (toneEnabled && noiseEnabled) {
                sample = m_toneGenerator.Value() & m_noiseGenerator.Value();
            } else if (toneEnabled) {
                sample = m_toneGenerator.Value();
            } else if (noiseEnabled) {
                sample = m_noiseGenerator.Value();
            } else {
                return 0.f; // No sound, return "center" value
            }

            // Convert int sample [0,1] to float [-1,1]
            auto finalSample = (sample * 2.f) - 1.f;

            // Apply volume
            finalSample *= volume;

            return finalSample;
        }

    private:
        bool m_toneEnabled{};
        bool m_noiseEnabled{};
        ToneGenerator& m_toneGenerator;
        NoiseGenerator& m_noiseGenerator;
        AmplitudeControl m_amplitudeControl;
    };

} // namespace

class PsgImpl {
public:
    PsgImpl();
    void Init();

    void SetBDIR(bool enable) { m_BDIR = enable; }
    void SetBC1(bool enable) { m_BC1 = enable; }
    bool BDIR() const { return m_BDIR; }
    bool BC1() const { return m_BC1; }

    void WriteDA(uint8_t value);
    uint8_t ReadDA();

    void Reset();
    void Update(cycles_t cycles);

    float Sample() const;

    void FrameUpdate(double frameTime);

private:
    void Clock();

    uint8_t Read(uint16_t address);
    void Write(uint16_t address, uint8_t value);

    enum class PsgMode {
        // Selected from BDIR (bit 1) and BC1 (bit 0) values
        Inactive,    // BDIR off BC1 off
        Read,        // BDIR off BC1 on
        Write,       // BDIR on  BC1 off
        LatchAddress // BDIR on  BC1 on
    };

    PsgMode m_mode = PsgMode::Inactive;

    bool m_BDIR{};
    bool m_BC1{};
    uint8_t m_DA{}; // Data/Address bus (DA7-DA0)
    uint8_t m_latchedAddress{};
    std::array<uint8_t, 16> m_registers{};
    Timer m_masterDivider{16}; // Input clock divided by 16
    std::array<ToneGenerator, 3> m_toneGenerators{};
    NoiseGenerator m_noiseGenerator{};
    EnvelopeGenerator m_envelopeGenerator{};
    std::array<PsgChannel, 3> m_channels;
};

PsgImpl::PsgImpl()
    : m_channels{PsgChannel{m_toneGenerators[0], m_noiseGenerator, m_envelopeGenerator},
                 PsgChannel{m_toneGenerators[1], m_noiseGenerator, m_envelopeGenerator},
                 PsgChannel{m_toneGenerators[2], m_noiseGenerator, m_envelopeGenerator}} {}

void PsgImpl::Init() {
    Reset();
}

void PsgImpl::WriteDA(uint8_t value) {
    m_DA = value;
}

uint8_t PsgImpl::ReadDA() {
    return m_DA;
}

void PsgImpl::Reset() {
    m_mode = {};
    m_DA = {};
    m_registers.fill(0);
    m_masterDivider.Reset();
    m_toneGenerators = {};
    m_noiseGenerator = {};
    m_envelopeGenerator = {};
}

void PsgImpl::Update(cycles_t cycles) {
    for (cycles_t cycle = 0; cycle < cycles; ++cycle) {
        Clock();
    }
}

void PsgImpl::FrameUpdate(double frameTime) {
    // Debug output
    static bool PsgImGui = false;
    IMGUI_CALL(Debug, ImGui::Checkbox("<<< Psg >>>", &PsgImGui));
    if (PsgImGui) {
        auto IndexToChannelName = [](auto index) {
            switch (index) {
            case 0:
                return "A";
            case 1:
                return "B";
            case 2:
                return "C";
            }
            return "";
        };

        auto ImGuiLoopLabel = [](const char* name, size_t index) {
            return FormattedString<>("%s##%d", name, (int)index);
        };

        const int NumHistoryValues = 5000;
        static std::array<PlotData<float, NumHistoryValues>, 3> channelHistories;
        static std::array<PlotData<float, NumHistoryValues>, 3> toneHistories;
        static std::array<PlotData<float, NumHistoryValues>, 3> noiseHistories;
        static std::array<PlotData<float, NumHistoryValues>, 3> volumeHistories;
        static PlotData<float, NumHistoryValues> envelopeHistory;

        for (size_t i = 0; i < m_channels.size(); ++i) {
            auto& channel = m_channels[i];

            IMGUI_CALL(Debug, ImGui::Text("Channel %s", IndexToChannelName(i)));

            IMGUI_CALL(Debug,
                       ImGui::Checkbox(ImGuiLoopLabel("Tone", i), &channel.OverrideToneEnabled));
            IMGUI_CALL(Debug,
                       ImGui::Checkbox(ImGuiLoopLabel("Noise", i), &channel.OverrideNoiseEnabled));

            auto& channelHistory = channelHistories[i];
            auto& toneHistory = toneHistories[i];
            auto& noiseHistory = noiseHistories[i];
            auto& volumeHistory = volumeHistories[i];

            if (frameTime > 0.f) {
                channelHistory.AddValue(channel.Sample());
                toneHistory.AddValue((float)channel.GetToneGenerator().Value());
                noiseHistory.AddValue((float)channel.GetNoiseGenerator().Value());
                volumeHistory.AddValue((float)channel.GetAmplitudeControl().Volume());
            }

            IMGUI_CALL(Debug, ImGui::PlotLines(ImGuiLoopLabel("Channel History", i),
                                               channelHistory.Values().data(),
                                               (int)channelHistory.Values().size(), 0, nullptr,
                                               -1.f, 1.f, ImVec2(0, 100.f)));

            IMGUI_CALL(Debug, ImGui::PlotLines(ImGuiLoopLabel("Tone History", i),
                                               toneHistory.Values().data(),
                                               (int)toneHistory.Values().size(), 0, nullptr, 0.f,
                                               1.f, ImVec2(0, 100.f)));

            IMGUI_CALL(Debug, ImGui::PlotLines(ImGuiLoopLabel("Noise History", i),
                                               noiseHistory.Values().data(),
                                               (int)toneHistory.Values().size(), 0, nullptr, 0.f,
                                               1.f, ImVec2(0, 100.f)));

            IMGUI_CALL(Debug, ImGui::PlotLines(ImGuiLoopLabel("Volume History", i),
                                               volumeHistory.Values().data(),
                                               (int)volumeHistory.Values().size(), 0, nullptr, 0.f,
                                               1.f, ImVec2(0, 100.f)));
        }

        IMGUI_CALL(Debug, ImGui::Text("General"));

        if (frameTime > 0.f) {
            envelopeHistory.AddValue(static_cast<float>(m_envelopeGenerator.Value()));
        }

        IMGUI_CALL(Debug, ImGui::PlotLines("Envelope", envelopeHistory.Values().data(),
                                           (int)envelopeHistory.Values().size(), 0, nullptr, 0.f,
                                           15.f, ImVec2(0, 100.f)));
    }
}

void PsgImpl::Clock() {
    auto ModeFromBDIRandBC1 = [](bool BDIR, bool BC1) -> PsgImpl::PsgMode {
        uint8_t value{};
        SetBits(value, 0b10, BDIR);
        SetBits(value, 0b01, BC1);
        return static_cast<PsgImpl::PsgMode>(value);
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

float PsgImpl::Sample() const {
    // Sample and mix each of the 3 channels
    float sample = 0.f;
    for (auto& channel : m_channels) {
        sample += channel.Sample();
    }
    sample /= 3.f;
    return sample;
}

uint8_t PsgImpl::Read(uint16_t address) {
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

void PsgImpl::Write(uint16_t address, uint8_t value) {
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
        if (ReadBits(value, 0b1100'0000) != 0)
            ErrorHandler::Unsupported("Not supporting I/O ports on PSG\n");

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

Psg::Psg() = default;
Psg::~Psg() = default;

void Psg::Init() {
    m_impl->Init();
}

void Psg::SetBDIR(bool enable) {
    m_impl->SetBDIR(enable);
}

void Psg::SetBC1(bool enable) {
    m_impl->SetBC1(enable);
}

bool Psg::BDIR() const {
    return m_impl->BDIR();
}

bool Psg::BC1() const {
    return m_impl->BC1();
}

void Psg::WriteDA(uint8_t value) {
    m_impl->WriteDA(value);
}

uint8_t Psg::ReadDA() {
    return m_impl->ReadDA();
}

void Psg::Reset() {
    m_impl->Reset();
}

void Psg::Update(cycles_t cycles) {
    m_impl->Update(cycles);
}

float Psg::Sample() const {
    return m_impl->Sample();
}

void Psg::FrameUpdate(double frameTime) {
    return m_impl->FrameUpdate(frameTime);
}
