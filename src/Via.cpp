#include "Via.h"
#include "BitOps.h"
#include "EngineClient.h"
#include "MemoryMap.h"

namespace {
    enum class ShiftRegisterMode {
        // There are actually many more modes, but I think Vectrex only uses one
        ShiftOutUnder02
    };

    namespace Register {
        enum Type {
            PortB = 0x0,
            PortA = 0x1,
            DataDirB = 0x2,
            DataDirA = 0x3,
            Timer1Low = 0x4,
            Timer1High = 0x5,
            Timer1LatchLow = 0x6,
            Timer1LatchHigh = 0x7,
            Timer2Low = 0x8,
            Timer2High = 0x9,
            Shift = 0xA,
            AuxCntl = 0xB,
            PeriphCntl = 0xC,
            InterruptFlag = 0xD,
            InterruptEnable = 0xE,
            PortANoHandshake = 0xF,
        };
    }

    namespace PortB {
        const uint8_t MuxDisabled = BITS(0);
        const uint8_t MuxSelMask = BITS(1, 2);
        const uint8_t MuxSelShift = 1;
        const uint8_t SoundBC1 = BITS(3);
        const uint8_t SoundBDir = BITS(4);
        const uint8_t Comparator = BITS(5);
        const uint8_t RampDisabled = BITS(7);
    } // namespace PortB

    namespace AuxCntl {
        const uint8_t ShiftRegisterModeMask = BITS(2, 3, 4);
        const uint8_t ShiftRegisterModeShift = 2;
        const uint8_t Timer2PulseCounting = BITS(5); // 1=pulse counting, 0=one-shot
        const uint8_t Timer1FreeRunning = BITS(6);   // 1=free running, 0=one-shot
        const uint8_t PB7Flag = BITS(7);             // 1=enable PB7 output

        inline ShiftRegisterMode GetShiftRegisterMode(uint8_t auxCntrl) {
            uint8_t result =
                ReadBitsWithShift(auxCntrl, ShiftRegisterModeMask, ShiftRegisterModeShift);
            ASSERT_MSG(result == 0b110,
                       "ShiftRegisterMode expected to only support ShiftOutUnder02");
            return ShiftRegisterMode::ShiftOutUnder02;
        }

        inline TimerMode GetTimer1Mode(uint8_t auxCntl) {
            return TestBits(auxCntl, Timer1FreeRunning) ? TimerMode::FreeRunning
                                                        : TimerMode::OneShot;
        }

        inline TimerMode GetTimer2Mode(uint8_t auxCntl) {
            return TestBits(auxCntl, Timer2PulseCounting) ? TimerMode::PulseCounting
                                                          : TimerMode::OneShot;
        }
    } // namespace AuxCntl

    namespace PeriphCntl {
        // CA1 -> SW7, 0=IRQ on low, 1=IRQ on high
        const uint8_t CA1 = BITS(0);

        // CA2 -> /ZERO, 110=low, 111=high
        const uint8_t CA2Mask = BITS(1, 2, 3);
        const uint8_t CA2Shift = 1;

        // CB1 -> nc, 0=IRQ on low, 1=IRQ on high
        const uint8_t CB1 = BITS(4);

        // CB2 -> /BLANK, 110=low, 111=high
        const uint8_t CB2Mask = BITS(5, 6, 7);
        const uint8_t CB2Shift = 5;

        inline bool IsZeroEnabled(uint8_t periphCntrl) {
            const uint8_t value = ReadBitsWithShift(periphCntrl, CA2Mask, CA2Shift);
            return value == 0b110;
        }

        inline bool IsBlankEnabled(uint8_t periphCntrl) {
            const uint8_t value = ReadBitsWithShift(periphCntrl, CB2Mask, CB2Shift);
            return value == 0b110;
        }
    } // namespace PeriphCntl

    namespace InterruptFlag {
        // How each IFR bit is cleared:
        // 0 CA2 interrupt flag            reading or writing port a i / o
        // 1 CA1 interrupt flag            reading or writing port a i / o
        // 2 shift register interrupt flag reading or writing shift register
        // 3 CB2 interrupt flag            reading or writing port b i / o
        // 4 CB1 interrupt flag            reading or writing port a i / o
        // 5 timer 2 interrupt flag        read t2 low or write t2 high
        // 6 timer 1 interrupt flag        read t1 count low or write t1 high
        // 7 irq status flag               write logic 0 to ier or ifr bit

        const uint8_t CA2 = BITS(0);
        const uint8_t CA1 = BITS(1);
        const uint8_t Shift = BITS(2);
        const uint8_t CB2 = BITS(3);
        const uint8_t CB1 = BITS(4);
        const uint8_t Timer2 = BITS(5);
        const uint8_t Timer1 = BITS(6);
        const uint8_t IrqEnabled = BITS(7);
    }; // namespace InterruptFlag

    namespace InterruptEnable {
        const uint8_t CA2 = BITS(0);
        const uint8_t CA1 = BITS(1);
        const uint8_t Shift = BITS(2);
        const uint8_t CB2 = BITS(3);
        const uint8_t CB1 = BITS(4);
        const uint8_t Timer2 = BITS(5);
        const uint8_t Timer1 = BITS(6);
        const uint8_t SetClearControl = BITS(7);
    } // namespace InterruptEnable

} // namespace

void Via::Init(MemoryBus& memoryBus) {
    memoryBus.ConnectDevice(*this, MemoryMap::Via.range);
}

void Via::Reset() {
    m_portB = m_portA = 0;
    m_dataDirB = m_dataDirA = 0;
    m_periphCntl = 0;
    m_interruptEnable = 0;

    SetBits(m_portB, PortB::RampDisabled, true);
}

void Via::Update(cycles_t cycles, const Input& input, RenderContext& renderContext) {
    // Update cached input state
    m_joystickButtonState = input.ButtonStateMask();

    // Analog input: update POT value if MUX is enabled, otherwise it keeps its last value
    const bool muxEnabled = !TestBits(m_portB, PortB::MuxDisabled);
    if (muxEnabled) {
        uint8_t muxSel = ReadBitsWithShift(m_portB, PortB::MuxSelMask, PortB::MuxSelShift);
        m_joystickPot = input.AnalogStateMask(muxSel);
    }

    // CA1 is set when joystick 2 button 4 is pressed, which is usually always on for peripherals
    // like the 3D imager goggles. Interrupt flag for CA1 is set when CA1 line goes from disabled to
    // enabled, and is cleared by read/write on Port A.
    const auto ca1Prev = m_ca1Enabled;
    m_ca1Enabled = input.IsButtonDown(1, 3);
    if (!ca1Prev && m_ca1Enabled)
        m_ca1InterruptFlag = true;

    // For cycle-accurate drawing, we update our timers, shift register, and beam movement 1 cycle
    // at a time
    cycles_t cyclesLeft = cycles;
    cycles = 1;
    while (cyclesLeft-- > 0) {
        m_timer1.Update(cycles);
        m_timer2.Update(cycles);
        m_shiftRegister.Update(cycles);

        // Shift register's CB2 line drives /BLANK
        //@TODO: check some flag on the shift register to know whether it's active
        if (m_shiftRegister.Enabled()) {
            m_blank = m_shiftRegister.CB2Active();
        }

        // If the Timer1 PB7 flag is set, then PB7 drives /RAMP
        if (m_timer1.PB7Flag()) {
            SetBits(m_portB, PortB::RampDisabled, !m_timer1.PB7SignalLow());
        }

        if (PeriphCntl::IsZeroEnabled(m_periphCntl)) {
            //@TODO: move beam towards 0,0 over time
            m_pos = {0.f, 0.f};
        }

        const auto lastPos = m_pos;
        Vector2 delta = {0.f, 0.f};

        // Integrators are enabled while RAMP line is active (low)
        bool integratorsEnabled = !TestBits(m_portB, PortB::RampDisabled);
        if (integratorsEnabled) {
            auto offset = Vector2{m_xyOffset, m_xyOffset};
            delta = (m_velocity + offset) / 128.f * static_cast<float>(cycles);
            m_pos += delta;
        }

        // We might draw even when integrators are disabled (e.g. drawing dots)
        bool drawingEnabled = !m_blank && (m_brightness > 0.f && m_brightness <= 128.f);
        if (drawingEnabled) {
            renderContext.lines.emplace_back(Line{lastPos, m_pos});
        }
    }
}

uint8_t Via::Read(uint16_t address) const {
    const uint16_t index = MemoryMap::Via.MapAddress(address);
    switch (index) {
    case Register::PortB: {
        uint8_t result = m_portB;

        // Set comparator bit to port A (DAC) value < joystick POT value
        int8_t portASigned = static_cast<int8_t>(m_portA);
        SetBits(result, PortB::Comparator, portASigned < m_joystickPot);

        return result;
    }
    case Register::PortA: {
        m_ca1InterruptFlag = false; // Cleared by read/write of Port A

        uint8_t result = m_portA;

        // Digital input
        if (!TestBits(m_portB, PortB::SoundBDir) && TestBits(m_portB, PortB::SoundBC1)) {
            if (m_dataDirA == 0) { // Input mode
                                   // @TODO: in this mode, we're reading the PSG's port A, not the
                                   // VIA's DAC, so this is probably wrong
                result = m_joystickButtonState;
            }
        }

        return result;
    }
    case Register::DataDirB:
        return m_dataDirB;

    case Register::DataDirA:
        return m_dataDirA;

    case Register::Timer1Low:
        return m_timer1.ReadCounterLow();

    case Register::Timer1High:
        return m_timer1.ReadCounterHigh();

    case Register::Timer1LatchLow:
        return m_timer1.ReadLatchLow();

    case Register::Timer1LatchHigh:
        return m_timer1.ReadLatchHigh();

    case Register::Timer2Low:
        return m_timer2.ReadCounterLow();

    case Register::Timer2High:
        return m_timer2.ReadCounterHigh();

    case Register::Shift:
        return m_shiftRegister.ReadValue();

    case Register::AuxCntl: {
        uint8_t auxCntl = 0;
        SetBits(auxCntl, 0b110 << AuxCntl::ShiftRegisterModeShift, true); //@HACK
        SetBits(auxCntl, AuxCntl::Timer1FreeRunning,
                m_timer1.TimerMode() == TimerMode::FreeRunning);
        SetBits(auxCntl, AuxCntl::Timer2PulseCounting,
                m_timer2.TimerMode() == TimerMode::PulseCounting);
        SetBits(auxCntl, AuxCntl::PB7Flag, m_timer1.PB7Flag());
        return auxCntl;
    }
    case Register::PeriphCntl:
        return m_periphCntl;

    case Register::InterruptFlag:
        return GetInterruptFlagValue();

    case Register::InterruptEnable:
        // FAIL_MSG("Read InterruptEnable not implemented");
        return m_interruptEnable;

    case Register::PortANoHandshake:
        FAIL_MSG("A without handshake not implemented yet");
        break;

    default:
        FAIL();
        break;
    }
    return 0;
}

void Via::Write(uint16_t address, uint8_t value) {

    auto UpdateIntegrators = [&] {
        const bool muxEnabled = !TestBits(m_portB, PortB::MuxDisabled);
        if (muxEnabled) {
            switch (ReadBitsWithShift(m_portB, PortB::MuxSelMask, PortB::MuxSelShift)) {
            case 0: // Y-axis integrator
                m_velocity.y = static_cast<int8_t>(m_portA);
                break;
            case 1: // X,Y Axis integrator offset
                m_xyOffset = static_cast<int8_t>(m_portA);
                break;
            case 2: // Z Axis (Vector Brightness) level
                m_brightness = m_portA;
                break;
            case 3: // Connected to sound output line via divider network
                //@TODO
                break;
            default:
                FAIL();
                break;
            }
        }
        // Always output to X-axis integrator
        m_velocity.x = static_cast<int8_t>(m_portA);
    };

    const uint16_t index = MemoryMap::Via.MapAddress(address);
    switch (index) {
    case Register::PortB:
        m_portB = value;
        UpdateIntegrators();
        break;

    case Register::PortA:
        m_ca1InterruptFlag = false; // Cleared by read/write of Port A

        // Port A is connected directly to the DAC, which in turn is connected to both a MUX with 4
        // outputs, and to the X-axis integrator.
        m_portA = value;
        if (m_dataDirA == 0xFF) {
            UpdateIntegrators();
        }
        break;

    case Register::DataDirB:
        m_dataDirB = value;
        break;

    case Register::DataDirA:
        m_dataDirA = value;
        ASSERT_MSG(m_dataDirA == 0 || m_dataDirA == 0xFF,
                   "Expecting DDR for A to be either all 0s or all 1s");
        break;

    case Register::Timer1Low:
        m_timer1.WriteCounterLow(value);
        break;

    case Register::Timer1High:
        m_timer1.WriteCounterHigh(value);
        break;

    case Register::Timer1LatchLow:
        m_timer1.WriteLatchLow(value);
        break;

    case Register::Timer1LatchHigh:
        m_timer1.WriteLatchHigh(value);
        break;

    case Register::Timer2Low:
        m_timer2.WriteCounterLow(value);
        break;

    case Register::Timer2High:
        m_timer2.WriteCounterHigh(value);
        break;

    case Register::Shift:
        m_shiftRegister.SetValue(value);
        break;

    case Register::AuxCntl: {
        // For now just read the shift register mode, which will assert if it's invalid/unexpected
        auto shiftRegisterMode = AuxCntl::GetShiftRegisterMode(value);
        (void)shiftRegisterMode;

        ASSERT_MSG(AuxCntl::GetTimer1Mode(value) == TimerMode::OneShot,
                   "t1 assumed always on one-shot mode");
        ASSERT_MSG(AuxCntl::GetTimer2Mode(value) == TimerMode::OneShot,
                   "t2 assumed always on one-shot mode");
        m_timer1.SetTimerMode(AuxCntl::GetTimer1Mode(value));
        m_timer2.SetTimerMode(AuxCntl::GetTimer2Mode(value));

        m_timer1.SetPB7Flag(TestBits(value, AuxCntl::PB7Flag));

    } break;

    case Register::PeriphCntl: {
        ASSERT_MSG(ReadBitsWithShift(value, PeriphCntl::CA2Mask, PeriphCntl::CA2Shift) == 0b110 ||
                       ReadBitsWithShift(value, PeriphCntl::CA2Mask, PeriphCntl::CA2Shift) == 0b111,
                   "Unexpected value for Zero bits");

        ASSERT_MSG(ReadBitsWithShift(value, PeriphCntl::CB2Mask, PeriphCntl::CB2Shift) == 0b110 ||
                       ReadBitsWithShift(value, PeriphCntl::CB2Mask, PeriphCntl::CB2Shift) == 0b111,
                   "Top 2 bits should always be 1 (right?)");

        m_periphCntl = value;
        if (!m_shiftRegister.Enabled()) {
            m_blank = PeriphCntl::IsBlankEnabled(m_periphCntl);
        }
    } break;

    case Register::InterruptFlag:
        // Clear interrupt flags for any bits that are enabled
        //@TODO: validate if this is the correct behaviour

        // Assert if trying to clear an interrupt we don't handle yet
#define ASSERT_UNHANDLED(flag)                                                                     \
    ASSERT_MSG(!TestBits(value, flag), "Write to clear interrupt not supported yet: %s", #flag)
        ASSERT_UNHANDLED(InterruptFlag::CA2);
        ASSERT_UNHANDLED(InterruptFlag::CB1);
        ASSERT_UNHANDLED(InterruptFlag::CB2);
#undef ASSERT_UNHANDLED

        if (TestBits(value, InterruptFlag::CA1))
            m_ca1InterruptFlag = false;
        if (TestBits(value, InterruptFlag::Shift))
            m_shiftRegister.SetInterruptFlag(false);
        if (TestBits(value, InterruptFlag::Timer2))
            m_timer2.SetInterruptFlag(false);
        if (TestBits(value, InterruptFlag::Timer1))
            m_timer1.SetInterruptFlag(false);
        break;

    case Register::InterruptEnable: {
        // When bit 7 = 0 : Each 1 in a bit position is cleared (disabled).
        // When bit 7 = 1 : Each 1 in a bit position enables that bit.
        // (Zeros in bit positions are left unchanged)
        SetBits(m_interruptEnable, (value & 0x7F), TestBits(value, BITS(7)));

        // Assert if trying to enable interrupt we don't handle yet
#define ASSERT_UNHANDLED(flag)                                                                     \
    ASSERT_MSG(!TestBits(m_interruptEnable, flag),                                                 \
               "Write to enable interrupt not supported yet: %s", #flag)
        ASSERT_UNHANDLED(InterruptFlag::CA2);
        ASSERT_UNHANDLED(InterruptFlag::CB1);
        ASSERT_UNHANDLED(InterruptFlag::CB2);
#undef ASSERT_UNHANDLED

    } break;

    case Register::PortANoHandshake:
        FAIL_MSG("A without handshake not implemented yet");
        break;

    default:
        FAIL();
        break;
    }
}

bool Via::IrqEnabled() const {
    return TestBits(GetInterruptFlagValue(), InterruptFlag::IrqEnabled);
}

uint8_t Via::GetInterruptFlagValue() const {
    uint8_t result = 0;
    SetBits(result, InterruptFlag::CA1, m_ca1InterruptFlag);
    SetBits(result, InterruptFlag::Shift, m_shiftRegister.InterruptFlag());
    SetBits(result, InterruptFlag::Timer2, m_timer2.InterruptFlag());
    SetBits(result, InterruptFlag::Timer1, m_timer1.InterruptFlag());

    // Set IRQ enable (bit 7) if any bits are on in both result and interruptEnable
    //@TODO: Shouldn't we only enable IRQ if any of the bits in IFR is also enabled in IER?
    // SetBits(result, InterruptFlag::IrqEnabled,
    //        TestBits(result, 0x7F) && TestBits(m_interruptEnable, 0x7F));

    // Enable IRQ if any IFR bits are also enabled in IER
    SetBits(result, InterruptFlag::IrqEnabled, ((result & m_interruptEnable) & 0x7F) != 0);

    return result;
}
