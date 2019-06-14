#pragma once
#include "engine/EngineClient.h"

class NullEngine {
public:
    void RegisterClient(IEngineClient& client);

    // Blocking call, returns when application is exited (window closed, etc.)
    bool Run(int argc, char** argv);
};
