#pragma once

#include "core/Pimpl.h"
#include <tuple>

struct RenderContext;

class GLRender {
public:
    GLRender();
    ~GLRender();

    std::tuple<int, int> GetMajorMinorVersion();
    void Initialize(bool enableGLDebugging);
    void Shutdown();
    void ResetOverlay(const char* file = nullptr);
    bool OnWindowResized(int windowWidth, int windowHeight);
    void RenderScene(double frameTime, const RenderContext& renderContext);

private:
    pimpl::Pimpl<class GLRenderImpl, 1024> m_impl;
};
