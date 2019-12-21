#pragma once

#include <algorithm>
#include <chrono>

class FrameTimer {
public:
    FrameTimer() { Reset(); }

    void FrameUpdate() {
        const auto currTime = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> diff = currTime - m_lastTime;
        const double realFrameTime = diff.count();
        m_lastTime = currTime;

        // FPS
        m_frames += 1;
        m_elapsedTime += realFrameTime;
        if (m_elapsedTime >= 1) {
            m_fps = m_frames / m_elapsedTime;
            m_frames = 0;
            m_elapsedTime = 0;
        }

        // Clamp
        m_frameTime = std::min(realFrameTime, MsToSec(100.0));
    }

    void Reset() { m_lastTime = std::chrono::high_resolution_clock::now(); }

    double GetFrameTime() const { return m_frameTime; }
    double GetFps() const { return m_fps; }

private:
    template <typename T>
    static T MsToSec(T ms) {
        return static_cast<T>(ms / 1000.0);
    }

    std::chrono::high_resolution_clock::time_point m_lastTime{};
    double m_frames{};
    double m_elapsedTime{};
    double m_frameTime{};
    double m_fps{};
};
