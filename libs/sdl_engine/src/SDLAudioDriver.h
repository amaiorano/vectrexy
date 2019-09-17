#pragma once
#include "core/Base.h"
#include "core/Pimpl.h"

class SDLAudioDriver {
public:
    SDLAudioDriver();
    ~SDLAudioDriver();

    void Initialize();
    void Shutdown();
    void Update(double frameTime);

    void SetVolume(float volume);
    float GetVolume();

    size_t GetSampleRate() const;
    float GetBufferUsageRatio() const;

    // Value in range [-1,1]
    void AddSample(float sample);
    void AddSamples(const float* samples, size_t size);

private:
    pimpl::Pimpl<class SDLAudioDriverImpl, 256> m_impl;
};
