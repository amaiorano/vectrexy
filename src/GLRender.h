#pragma once

#include <tuple>

namespace GLRender {
    void Initialize(int screenWidth, int screenHeight);
    void Shutdown();
    std::tuple<int, int> GetMajorMinorVersion();
    bool SetViewport(int windowWidth, int windowHeight);
    void PreRender();
    void RenderScene(double frameTime);
} // namespace GLRender
