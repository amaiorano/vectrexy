#pragma once

class IEngineClient {
public:
    virtual bool Init(int argc, char** argv) = 0;
    virtual bool Update(double deltaTime) = 0;
    virtual void Shutdown() = 0;
};

class SDLEngine {
public:
    void RegisterClient(IEngineClient& client);

    // Blocking call, returns when application is exited (window closed, etc.)
    bool Run(int argc, char** argv);
};
