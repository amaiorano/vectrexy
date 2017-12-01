#pragma once

#include "Base.h"
#include "BitOps.h"
#include "Line.h"
#include "Vector2.h"
#include <array>
#include <vector>

class Display {
public:
    void Clear();
    void DrawLines(const std::vector<Line>& lines);
};

class Input {
public:
    // Read by emulator
    uint8_t ButtonStateMask() const { return m_joystickButtonState; }
    int8_t AnalogStateMask(int joyAxis) const { return m_joystickAnalogState[joyAxis]; }

    // Set by engine
    void SetButton(uint8_t joystickIndex, uint8_t buttonIndex, bool enable) {
        assert(joystickIndex < 2);
        assert(buttonIndex < 4);
        const uint8_t mask = 1 << (buttonIndex + joystickIndex * 4);
        SetBits(m_joystickButtonState, mask, enable == false);
    }
    void SetAnalogAxisX(int joystickIndex, int8_t value) {
        m_joystickAnalogState[joystickIndex * 2 + 0] = value;
    }

    void SetAnalogAxisY(int joystickIndex, int8_t value) {
        m_joystickAnalogState[joystickIndex * 2 + 1] = value;
    }

private:
    // Buttons 4,3,2,1 for joy 0 in bottom bits, and for joy 1 in top bits
    uint8_t m_joystickButtonState = 0xFF; // Bits on if not pressed
    // X1, Y1, X2, Y2
    std::array<int8_t, 4> m_joystickAnalogState = {0};
};

class EmuEvent {
public:
    enum class Type { BreakIntoDebugger };
    Type type;
};
using EmuEvents = std::vector<EmuEvent>;

class IEngineClient {
public:
    virtual bool Init(int argc, char** argv) = 0;
    virtual bool Update(double frameTime, const Input& input, const EmuEvents& emuEvents) = 0;
    virtual void Render(double frameTime, Display& display) = 0;
    virtual void Shutdown() = 0;
};

// Set focus on the main window
void SetFocusMainWindow();
void SetFocusConsole();