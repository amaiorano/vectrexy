#pragma once
#include "Base.h"
#include "Pimpl.h"

class SDLAudioDriver {
public:
    SDLAudioDriver();
    ~SDLAudioDriver();

    void Initialize();
    void Shutdown();

    size_t GetSampleRate() const;
    float GetBufferUsageRatio() const;

    void AddSampleF32(float sample);

private:
    pimpl::Pimpl<class SDLAudioDriverImpl, 256> m_impl;
};
