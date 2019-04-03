#pragma once

#include "emulator/EngineTypes.h"
#include "engine/Options.h"

struct EmuContext {
    std::reference_wrapper<EmuEvents> emuEvents;
    std::reference_wrapper<Options> options;
};

class IEngineClient {
public:
    virtual bool Init(std::shared_ptr<IEngineService>& engineService, int argc, char** argv) = 0;

    virtual bool FrameUpdate(double frameTime, const Input& input, const EmuContext& emuContext,
                             RenderContext& renderContext, AudioContext& audioContext) = 0;

    virtual void Shutdown() = 0;
};
