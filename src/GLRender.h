#pragma once

#include <tuple>

struct RenderContext;

namespace GLRender {
    std::tuple<int, int> GetMajorMinorVersion();
    bool EnableGLDebugging();
    void Initialize();
    void Shutdown();
    void ResetOverlay(const char* file = nullptr);
    bool OnWindowResized(int windowWidth, int windowHeight);
    void RenderScene(double frameTime, const RenderContext& renderContext);
} // namespace GLRender
