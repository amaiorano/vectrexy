#pragma once

#include "Options.h"
#include "emulator/EngineTypes.h"

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

class IEngineClient {
public:
    virtual bool Init(int argc, char** argv) = 0;
    virtual bool FrameUpdate(double frameTime, const Input& input, const EmuContext& emuContext,
                             RenderContext& renderContext, AudioContext& audioContext) = 0;
    virtual void Shutdown() = 0;
};
