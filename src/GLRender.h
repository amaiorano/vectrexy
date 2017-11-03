#pragma once

#include <tuple>

namespace GLRender {
    void Initialize();
    void Shutdown();
    std::tuple<int, int> GetMajorMinorVersion();
    bool SetViewport(int windowWidth, int windowHeight);
    void PreRender();
    void RenderScene(double frameTime);
} // namespace GLRender
