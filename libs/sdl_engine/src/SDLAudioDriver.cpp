#include "SDLAudioDriver.h"
#include "core/CircularBuffer.h"
#include "core/Gui.h"
#include "core/Stream.h"
#include "engine/Paths.h"
#include <SDL.h>
#include <SDL_audio.h>
#include <array>

namespace OutputRawAudioFileStream {
    constexpr bool Enabled = true;

    // Whether to output the source samples or the target samples
    constexpr bool SourceSamples = false;

}; // namespace OutputRawAudioFileStream

namespace {
    template <SDL_AudioFormat Format>
    struct AudioFormat;

    template <>
    struct AudioFormat<AUDIO_S16> {
        using Type = int16_t;
        static Type Remap(float ratio) {
            return static_cast<Type>(ratio * (std::numeric_limits<Type>::max() - 1));
        }
    };

    template <>
    struct AudioFormat<AUDIO_U16> {
        using Type = uint16_t;
        static Type Remap(float ratio) {
            return static_cast<Type>(((ratio + 1.f) / 2.f) * std::numeric_limits<Type>::max());
        }
    };

    template <>
    struct AudioFormat<AUDIO_F32> {
        using Type = float;
        static Type Remap(float ratio) { return ratio; }
    };
} // namespace

class SDLAudioDriverImpl {
public:
    static const int kSampleRate = 44100;
    static const SDL_AudioFormat kSampleFormat = AUDIO_S16;
    // static const SDL_AudioFormat kSampleFormat = AUDIO_U16;
    // static const SDL_AudioFormat kSampleFormat = AUDIO_F32;
    static const int kNumChannels = 1;
    static const int kSamplesPerCallback = 1024;

    using CurrAudioFormat = AudioFormat<kSampleFormat>;
    using SampleFormatType = CurrAudioFormat::Type;

    static std::string RawAudioFileName() {
        auto AudioFormatString = [](SDL_AudioFormat format) {
            switch (format) {
            case AUDIO_S16:
                return "S16";
            case AUDIO_U16:
                return "U16";
            case AUDIO_F32:
                return "F32";
            }
            FAIL_MSG("Missing/unknown audio format");
            return "NA";
        };

        std::string result =
            FormattedString("RawAudio_%s_%dhz_%dch.raw", AudioFormatString(kSampleFormat),
                            kSampleRate, kNumChannels)
                .Value();
        return result;
    }

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
        // No changes allowed, meaning SDL will take care of converting our samples in our desired
        // format to the actual target format.
        int allowedChanges = 0;
        m_audioDeviceID = SDL_OpenAudioDevice(nullptr, 0, &desired, &actual, allowedChanges);
        m_audioSpec = desired;

        if (m_audioDeviceID == 0)
            FAIL_MSG("Failed to open audio device (error code %d)", SDL_GetError());

        // Set buffer size as a function of the latency we allow
        const float kDesiredLatencySecs = 50 / 1000.0f;
        const float desiredLatencySamples = kDesiredLatencySecs * GetSampleRate();
        const auto bufferSize = static_cast<size_t>(
            desiredLatencySamples * 2); // We wait until buffer is 50% full to start playing
        m_samples.Init(bufferSize);

        if constexpr (OutputRawAudioFileStream::Enabled) {
            m_rawAudioOutputFS.Open(Paths::devDir / RawAudioFileName(), "wb");
        }

        m_paused = false;
        SetPaused(true);
    }

    void Shutdown() {
        SDL_CloseAudioDevice(m_audioDeviceID);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_rawAudioOutputFS.Close();
    }

    void Update(double /*frameTime*/) {
        AdjustBufferFlow();

        // Debug output
        static bool SDLAudioDriverImGui = false;
        IMGUI_CALL(Debug, ImGui::Checkbox("<<< SDLAudioDriver >>>", &SDLAudioDriverImGui));
        if (SDLAudioDriverImGui) {
            static std::array<float, 10000> bufferUsageHistory;
            static std::array<float, 10000> pauseHistory;
            static int index = 0;

            bufferUsageHistory[index] = GetBufferUsageRatio();
            pauseHistory[index] = m_paused ? 0.f : 1.f;
            index = (index + 1) % bufferUsageHistory.size();
            if (index == 0) {
                std::fill(bufferUsageHistory.begin(), bufferUsageHistory.end(), 0.f);
                std::fill(pauseHistory.begin(), pauseHistory.end(), 0.f);
            }

            IMGUI_CALL(Debug, ImGui::PlotLines("Buffer Usage", bufferUsageHistory.data(),
                                               (int)bufferUsageHistory.size(), 0, nullptr, 0.f, 1.f,
                                               ImVec2(0, 100.f)));

            IMGUI_CALL(Debug,
                       ImGui::PlotLines("Unpaused", pauseHistory.data(), (int)pauseHistory.size(),
                                        0, nullptr, 0.f, 1.f, ImVec2(0, 100.f)));

            IMGUI_CALL(Debug, ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                                                    m_paused ? IM_COL32(255, 0, 0, 255)
                                                             : IM_COL32(255, 255, 0, 255)));
            IMGUI_CALL(Debug, ImGui::ProgressBar(GetBufferUsageRatio(), ImVec2(-1, 100)));
            IMGUI_CALL(Debug, ImGui::PopStyleColor());
        }
    }

    void SetVolume(float volume) { m_volume = volume; }

    float GetVolume() { return m_volume; }

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

    void AdjustBufferFlow() {
        // Unpause when buffer is half full; pause if almost depleted to give buffer a chance to
        // fill up again.
        const auto bufferUsageRatio = GetBufferUsageRatio();
        if (bufferUsageRatio >= 0.5f) {
            SetPaused(false);
        } else if (bufferUsageRatio < 0.1f) {
            SetPaused(true);
        }
    }

    void AddSample(float sample) {
        assert(sample >= -1.0f && sample <= 1.0f);

        sample *= m_volume;

        auto targetSample = CurrAudioFormat::Remap(sample);

        SDL_LockAudioDevice(m_audioDeviceID);
        m_samples.PushBack(targetSample);
        SDL_UnlockAudioDevice(m_audioDeviceID);

        if constexpr (OutputRawAudioFileStream::Enabled &&
                      OutputRawAudioFileStream::SourceSamples) {
            m_rawAudioOutputFS.WriteValue(sample);
        }
    }

    void AddSamples(const float* samples, size_t size) {
        for (size_t i = 0; i < size; ++i)
            AddSample(samples[i]);
    }

private:
    static void AudioCallback(void* userData, Uint8* byteStream, int byteStreamLength) {
        auto audioDriver = reinterpret_cast<SDLAudioDriverImpl*>(userData);
        auto stream = reinterpret_cast<SampleFormatType*>(byteStream);

        size_t numSamplesToRead = byteStreamLength / sizeof(SampleFormatType);

        //@TODO: sync access to m_samples with a mutex here
        size_t numSamplesRead = audioDriver->m_samples.PopFront(stream, numSamplesToRead);

        // If we haven't written enough samples, fill out the rest with the last sample
        // written. This will usually hide the error.
        if (numSamplesRead < numSamplesToRead) {
            SampleFormatType lastSample = numSamplesRead == 0 ? 0 : stream[numSamplesRead - 1];
            std::fill_n(stream + numSamplesRead, numSamplesToRead - numSamplesRead, lastSample);
        }

        if constexpr (OutputRawAudioFileStream::Enabled &&
                      !OutputRawAudioFileStream::SourceSamples) {
            for (size_t i = 0; i < numSamplesToRead; ++i) {
                audioDriver->m_rawAudioOutputFS.WriteValue(stream[i]);
            }
        }
    }

    SDL_AudioDeviceID m_audioDeviceID{0};
    SDL_AudioSpec m_audioSpec;
    CircularBuffer<SampleFormatType> m_samples;
    FileStream m_rawAudioOutputFS;
    bool m_paused;
    float m_volume{1.f};
};

SDLAudioDriver::SDLAudioDriver() = default;
SDLAudioDriver::~SDLAudioDriver() = default;

void SDLAudioDriver::Initialize() {
    m_impl->Initialize();
}

void SDLAudioDriver::Shutdown() {
    m_impl->Shutdown();
}

void SDLAudioDriver::Update(double frameTime) {
    m_impl->Update(frameTime);
}

void SDLAudioDriver::SetVolume(float volume) {
    m_impl->SetVolume(volume);
}
float SDLAudioDriver::GetVolume() {
    return m_impl->GetVolume();
}

size_t SDLAudioDriver::GetSampleRate() const {
    return m_impl->GetSampleRate();
}

float SDLAudioDriver::GetBufferUsageRatio() const {
    return m_impl->GetBufferUsageRatio();
}

void SDLAudioDriver::AddSample(float sample) {
    m_impl->AddSample(sample);
}

void SDLAudioDriver::AddSamples(const float* samples, size_t size) {
    m_impl->AddSamples(samples, size);
}
