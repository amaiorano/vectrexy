#pragma once

#include "Base.h"
#include "BitOps.h"
#include "FileSystem.h"
#include "Line.h"
#include "Options.h"
#include "Vector2.h"
#include <array>
#include <functional>
#include <vector>

class Input {
public:
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

    // Read by emulator
    uint8_t ButtonStateMask() const { return m_joystickButtonState; }
    int8_t AnalogStateMask(int joyAxis) const { return m_joystickAnalogState[joyAxis]; }

    bool IsButtonDown(uint8_t joystickIndex, uint8_t buttonIndex) const {
        assert(joystickIndex < 2);
        assert(buttonIndex < 4);
        const uint8_t mask = 1 << (buttonIndex + joystickIndex * 4);
        return TestBits(m_joystickButtonState, mask) == false;
    }

private:
    // Buttons 4,3,2,1 for joy 0 in bottom bits, and for joy 1 in top bits
    uint8_t m_joystickButtonState = 0xFF; // Bits on if not pressed
    // X1, Y1, X2, Y2
    std::array<int8_t, 4> m_joystickAnalogState = {0};
};

class EmuEvent {
public:
    struct BreakIntoDebugger {};
    struct Reset {};
    struct OpenRomFile {
        fs::path path{}; // If not set, use open file dialog
    };

    using Type = std::variant<BreakIntoDebugger, Reset, OpenRomFile>;
    Type type;
};
using EmuEvents = std::vector<EmuEvent>;

struct EmuContext {
    std::reference_wrapper<EmuEvents> emuEvents;
    std::reference_wrapper<Options> options;
};

struct RenderContext {
    std::vector<Line> lines; // Lines to draw this frame
};

struct AudioContext {
    AudioContext(float cpuCyclesPerAudioSample)
        : CpuCyclesPerAudioSample(cpuCyclesPerAudioSample) {}

    const float CpuCyclesPerAudioSample;
    std::vector<float> samples; // Samples produced this frame
};

class IEngineClient {
public:
    virtual bool Init(int argc, char** argv) = 0;
    virtual bool FrameUpdate(double frameTime, const Input& input, const EmuContext& emuContext,
                             RenderContext& renderContext, AudioContext& audioContext) = 0;
    virtual void Shutdown() = 0;
};

// Set focus on the main window
void SetFocusMainWindow();
void SetFocusConsole();

void ResetOverlay(const char* file = nullptr);
