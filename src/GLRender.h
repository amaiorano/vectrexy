#pragma once

#include <tuple>

namespace GLRender {
    void Initialize();
    void Shutdown();
    std::tuple<int, int> GetMajorMinorVersion();
    void ResetOverlay(const char* file = nullptr);
    bool OnWindowResized(int windowWidth, int windowHeight);
    void RenderScene(double frameTime);
} // namespace GLRender
