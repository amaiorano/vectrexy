#include "SDLAudioDriver.h"
#include "CircularBuffer.h"
#include "Stream.h"
#include <SDL.h>
#include <SDL_audio.h>

#define OUTPUT_RAW_AUDIO_FILE_STREAM 0

namespace {
    template <SDL_AudioFormat Format>
    struct FormatToType;
    template <>
    struct FormatToType<AUDIO_S16> {
        typedef int16_t Type;
    };
    template <>
    struct FormatToType<AUDIO_U16> {
        typedef uint16_t Type;
    };
    template <>
    struct FormatToType<AUDIO_F32> {
        typedef float Type;
    };
} // namespace

class SDLAudioDriverImpl {
public:
    static const int kSampleRate = 44100;
    static const SDL_AudioFormat kSampleFormat = AUDIO_S16; // Apparently supported by all drivers?
    // static const SDL_AudioFormat kSampleFormat = AUDIO_U16;
    // static const SDL_AudioFormat kSampleFormat = AUDIO_F32;
    static const int kNumChannels = 1;
    static const int kSamplesPerCallback = 1024;

    typedef FormatToType<kSampleFormat>::Type SampleFormatType;

    SDLAudioDriverImpl()
        : m_audioDeviceID(0) {}

    ~SDLAudioDriverImpl() { Shutdown(); }

    void Initialize() {
        SDL_InitSubSystem(SDL_INIT_AUDIO);

        SDL_AudioSpec desired;
        SDL_zero(desired);
        desired.freq = kSampleRate;
        desired.format = kSampleFormat;
        desired.channels = kNumChannels;
        desired.samples = kSamplesPerCallback;
        desired.callback = AudioCallback;
        desired.userdata = this;

        SDL_AudioSpec actual;
        m_audioDeviceID = SDL_OpenAudioDevice(NULL, 0, &desired, &actual /*NULL*/ /*&m_audioSpec*/,
                                              SDL_AUDIO_ALLOW_ANY_CHANGE);
        m_audioSpec = desired;

        if (m_audioDeviceID == 0)
            FAIL_MSG("Failed to open audio device (error code %d)", SDL_GetError());

        // Set buffer size as a function of the latency we allow
        const float kDesiredLatencySecs = 50 / 1000.0f;
        const float desiredLatencySamples = kDesiredLatencySecs * GetSampleRate();
        const size_t bufferSize = static_cast<size_t>(
            desiredLatencySamples * 2); // We wait until buffer is 50% full to start playing
        m_samples.Init(bufferSize);

#if OUTPUT_RAW_AUDIO_FILE_STREAM
        m_rawAudioOutputFS.Open("RawAudio.raw", "wb");
#endif

        SetPaused(true);
    }

    void Shutdown() {
        m_rawAudioOutputFS.Close();

        SDL_CloseAudioDevice(m_audioDeviceID);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    size_t GetSampleRate() const { return m_audioSpec.freq; }

    float GetBufferUsageRatio() const {
        return static_cast<float>(m_samples.UsedSize()) / m_samples.TotalSize();
    }

    void SetPaused(bool paused) {
        if (paused != m_paused) {
            m_paused = paused;
            SDL_PauseAudioDevice(m_audioDeviceID, m_paused ? 1 : 0);
        }
    }

    void AddSampleF32(float sample) {
        assert(sample >= 0.0f && sample <= 1.0f);
        //@TODO: This multiply is wrong for signed format types (S16, S32)
        float targetSample = sample * std::numeric_limits<SampleFormatType>::max();

        SDL_LockAudioDevice(m_audioDeviceID);
        m_samples.PushBack(static_cast<SampleFormatType>(targetSample));
        SDL_UnlockAudioDevice(m_audioDeviceID);

        // Unpause when buffer is half full; pause if almost depleted to give buffer a chance to
        // fill up again.
        const auto bufferUsageRatio = GetBufferUsageRatio();
        if (bufferUsageRatio >= 0.5f) {
            SetPaused(false);
        } else if (bufferUsageRatio < 0.1f) {
            SetPaused(true);
        }

#if OUTPUT_RAW_AUDIO_FILE_STREAM
        m_rawAudioOutputFS.WriteValue(sample);
#endif
    }

private:
    static void AudioCallback(void* userData, Uint8* byteStream, int byteStreamLength) {
        auto audioDriver = reinterpret_cast<SDLAudioDriverImpl*>(userData);
        auto stream = reinterpret_cast<SampleFormatType*>(byteStream);

        size_t numSamplesToRead = byteStreamLength / sizeof(SampleFormatType);

        size_t numSamplesRead = audioDriver->m_samples.PopBack(stream, numSamplesToRead);

        // If we haven't written enough samples, fill out the rest with the last sample
        // written. This will usually hide the error.
        if (numSamplesRead < numSamplesToRead) {
            SampleFormatType lastSample = numSamplesRead == 0 ? 0 : stream[numSamplesRead - 1];
            std::fill_n(stream + numSamplesRead, numSamplesToRead - numSamplesRead, lastSample);
        }
    }

    SDL_AudioDeviceID m_audioDeviceID;
    SDL_AudioSpec m_audioSpec;
    CircularBuffer<SampleFormatType> m_samples;
    FileStream m_rawAudioOutputFS;
    bool m_paused;
};

SDLAudioDriver::SDLAudioDriver() = default;
SDLAudioDriver::~SDLAudioDriver() = default;

void SDLAudioDriver::Initialize() {
    m_impl->Initialize();
}

void SDLAudioDriver::Shutdown() {
    m_impl->Shutdown();
}

size_t SDLAudioDriver::GetSampleRate() const {
    return m_impl->GetSampleRate();
}

float SDLAudioDriver::GetBufferUsageRatio() const {
    return m_impl->GetBufferUsageRatio();
}

void SDLAudioDriver::AddSampleF32(float sample) {
    m_impl->AddSampleF32(sample);
}
