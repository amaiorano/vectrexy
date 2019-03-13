#pragma once

#include "EngineTypes.h"

class IEngineClient {
public:
    virtual bool Init(int argc, char** argv) = 0;
    virtual bool FrameUpdate(double frameTime, const Input& input, const EmuContext& emuContext,
                             RenderContext& renderContext, AudioContext& audioContext) = 0;
    virtual void Shutdown() = 0;
};
