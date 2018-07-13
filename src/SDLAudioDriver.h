#pragma once
#include "Base.h"
#include "Pimpl.h"

class SDLAudioDriver {
public:
    SDLAudioDriver();
    ~SDLAudioDriver();

    void Initialize();
    void Shutdown();
    void Update(double frameTime);

    size_t GetSampleRate() const;
    float GetBufferUsageRatio() const;

    // Value in range [0,1]
    void AddSample(float sample);
    void AddSamples(const float* samples, size_t size);

private:
    pimpl::Pimpl<class SDLAudioDriverImpl, 256> m_impl;
};
