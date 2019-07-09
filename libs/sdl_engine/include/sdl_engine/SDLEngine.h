#pragma once
#include "core/BitOps.h"
#include "core/Pimpl.h"
#include "engine/EngineClient.h"

class SDLEngine {
public:
    SDLEngine();
    ~SDLEngine();

    void RegisterClient(IEngineClient& client);

    // Blocking call, returns when application is exited (window closed, etc.)
    bool Run(int argc, char** argv);

private:
    pimpl::Pimpl<class SDLEngineImpl, 4096> m_impl;
};
