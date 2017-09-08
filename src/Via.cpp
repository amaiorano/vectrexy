#include "Via.h"
#include "BitOps.h"
#include "MemoryMap.h"

namespace {
    namespace PortB {
        const uint8_t MuxDisabled = BITS(0);
        const uint8_t MuxSelMask = BITS(1, 2);
        const uint8_t MuxSelShift = 1; //???
        const uint8_t RampDisabled = BITS(7);
    } // namespace PortB

    namespace AuxCntl {
        const uint8_t Timer2FreeRunning = BITS(5);
        const uint8_t Timer1FreeRunning = BITS(6);
        const uint8_t PB7Enabled = BITS(7);
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

        inline bool ZeroEnabled(uint8_t periphCntrl) {
            const uint8_t value = ReadBitsWithShift(periphCntrl, CA2Mask, CA2Shift);
            ASSERT_MSG(value == 0b110 || value == 0b111, "Top 2 bits should always be 1 (right?)");
            return value == 0b110;
        }

        inline bool BlankEnabled(uint8_t periphCntrl) {
            const uint8_t value = ReadBitsWithShift(periphCntrl, CB2Mask, CB2Shift);
            ASSERT_MSG(value == 0b110 || value == 0b111, "Top 2 bits should always be 1 (right?)");
            return value == 0b110;
        }

    } // namespace PeriphCntl

    namespace InterruptFlag {
        const uint8_t Timer2 = BITS(5);
        const uint8_t Timer1 = BITS(6);
    }; // namespace InterruptFlag

} // namespace

void Via::Init(MemoryBus& memoryBus) {
    memoryBus.ConnectDevice(*this, MemoryMap::Via.range);
    m_portB = m_portA = 0;
    m_dataDirB = m_dataDirA = 0;
    m_timer1Low = m_timer1High = 0;
    m_timer1LatchLow = m_timer1LatchHigh = 0;
    m_timer2Low = m_timer2High = 0;
    m_shift = 0;
    m_auxCntl = 0;
    m_periphCntl = 0;
    m_interruptFlag = 0;
    m_interruptEnable = 0;
}

void Via::Update(cycles_t cycles) {
    m_timer1.Update(cycles);

    // Update timer 1 interrupt flag (bit 6)
    SetBits(m_interruptFlag, InterruptFlag::Timer1, m_timer1.InterruptEnabled());
    // m_interruptFlag |= (m_timer1.InterruptEnabled() ? InterruptFlag::Timer1 : 0);

    //@TODO: if (VIA_aux_cntl bit 7 set), need to set /RAMP based on timer 1's PB7 value
    if ((m_auxCntl & BITS(7)) != 0) {
        SetBits(m_portB, PortB::RampDisabled, !m_timer1.PB7Enabled());
        // m_portB |= (m_timer1.PB7Enabled() ? 0 : PortB::RampDisabled);
    }

    // Integrators are enabled while RAMP line is active (low)
    // bool integratorEnabled = (B & PortB::Ramp) == 0; // /RAMP

    // if (integratorEnabled) {
    //    Vector2 integratorInput;
    //    integratorInput.x = (m_velocity.x - 128.f) * (10.f / 256.f);
    //    integratorInput.y = (m_velocity.y - 128.f) * (10.f / 256.f);

    //    float integratorOffset = (m_xyOffset - 128.f) * (10.f / 256.f);

    //    Vector2 targetPos;
    //    targetPos.x =
    //        -((10000.0f * (integratorInput.x - integratorOffset) * (float)deltaTime) + m_pos.x);
    //    targetPos.y =
    //        -((10000.0f * (integratorInput.y - integratorOffset) * (float)deltaTime) + m_pos.y);

    //    bool drawEnabled = !m_blank && m_brightness > 0.f;

    //    if (drawEnabled) {
    //        // printf("Added line!\n");
    //        m_lines.emplace_back(Line{m_pos, targetPos});
    //    }

    //    m_pos = targetPos;
    //}
}

uint8_t Via::Read(uint16_t address) const {
    const uint16_t index = MemoryMap::Via.MapAddress(address);
    switch (index) {
    case 0x0:
        return m_portB;
    case 0x1:
        return m_portA;
    case 0x2:
        return m_dataDirB;
    case 0x3:
        return m_dataDirA;
    case 0x4:
        return m_timer1Low;
    case 0x5:
        return m_timer1High;
    case 0x6:
        FAIL_MSG("Not implemented. Not sure we need this.");
        return m_timer1LatchLow;
    case 0x7:
        FAIL_MSG("Not implemented. Not sure we need this.");
        return m_timer1LatchHigh;
    case 0x8:
        FAIL_MSG("Not implemented. Not sure we need this.");
        // return m_timer2Low;
        return 0;
    case 0x9:
        FAIL_MSG("Not implemented. Not sure we need this. 2");
        // return m_timer2High;
        return 0;
    case 0xA:
        return m_shift;
    case 0xB:
        return m_auxCntl;
    case 0xC:
        return m_periphCntl;
    case 0xD:
        return m_interruptFlag;
    case 0xE:
        FAIL_MSG("Not implemented");
        return m_interruptEnable;
    case 0xF:
        FAIL_MSG("A without handshake not implemented yet");
        break;
    default:
        FAIL();
        break;
    }
    return 0;
}

void Via::Write(uint16_t address, uint8_t value) {
    const uint16_t index = MemoryMap::Via.MapAddress(address);
    switch (index) {
    case 0:
        m_portB = value;
        goto UPDATE_INTEGRATORS;
        break;
    case 0x1: {
        // Port A is connected directly to the DAC, which in turn is connected to both a MUX with 4
        // outputs, and to the X-axis integrator.
        m_portA = value;

    UPDATE_INTEGRATORS:

        const bool muxEnabled = !TestBits(m_portB, PortB::MuxDisabled);
        if (muxEnabled) {
            switch (ReadBitsWithShift(m_portB, PortB::MuxSelMask, PortB::MuxSelShift)) {
            case 0:
                // Y-axis integrator
                // printf("Writing Y-axis integrator value!\n");
                m_velocity.y = m_portA;
                break;

            case 1:
                // X,Y Axis integrator offset
                m_xyOffset = m_portA;
                break;

            case 2:
                // Z Axis (Vector Brightness) level
                m_brightness = m_portA;
                break;

            case 3:
                // Connected to sound output line via divider network
                //@TODO
                break;

            default:
                FAIL();
                break;
            }
        } else {
            // printf("Writing X-axis integrator value!\n");
            // MUX disabled so we output to X-axis integrator
            m_velocity.x = m_portA;
        }
    } break;
    case 0x2:
        m_dataDirB = value;
        break;
    case 0x3:
        m_dataDirA = value;
        break;
    case 0x4:
        m_timer1Low = value;
        break;
    case 0x5:
        m_timer1High = value;
        break;
    case 0x6:
        m_timer1LatchLow = value;
        break;
    case 0x7:
        m_timer1LatchHigh = value;
        break;
    case 0x8:
        // m_timer2Low = value;
        m_timer1.SetCounterLow(value);
        break;
    case 0x9:
        // m_timer2High = value;
        m_timer1.SetCounterHigh(value);
        break;
    case 0xA:
        m_shift = value;
        break;
    case 0xB:
        ASSERT_MSG(ReadBits(value, AuxCntl::Timer1FreeRunning | AuxCntl::Timer2FreeRunning) == 0,
                   "t1 and t2 assumed to be always in one-shot mode");
        m_auxCntl = value;
        break;
    case 0xC: {
        m_periphCntl = value;

        if (PeriphCntl::ZeroEnabled(m_periphCntl))
            m_pos = {0.f, 0.f};

        m_blank = PeriphCntl::BlankEnabled(m_periphCntl);
    } break;
    case 0xD:
        FAIL_MSG("Not implemented");
        m_interruptFlag = value;
        break;
    case 0xE:
        FAIL_MSG("Not implemented");
        m_interruptEnable = value;
        break;
    case 0xF:
        FAIL_MSG("A without handshake not implemented yet");
        break;
    default:
        FAIL();
        break;
    }
}
