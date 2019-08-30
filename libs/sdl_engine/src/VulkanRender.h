#pragma once

#include "core/Pimpl.h"
#include <functional>
#include <vector>
#include <vulkan/vulkan.h>

struct RenderContext;

class VulkanRender {
public:
    VulkanRender();
    ~VulkanRender();

    // Function that returns the list of desired extensions
    using GetInstanceExtensionsFunc = std::function<std::vector<const char*>()>;

    // Function that creates and returns a VkSurfaceKHR for the input VkInstance
    using CreateSurfaceFunc = std::function<VkSurfaceKHR(VkInstance)>;

    void Initialize(const GetInstanceExtensionsFunc& getInstanceExtensions,
                    const CreateSurfaceFunc& createSurface);

    void Shutdown();
    void ResetOverlay(const char* file = nullptr);
    bool OnWindowResized(int windowWidth, int windowHeight);
    void RenderScene(double frameTime, const RenderContext& renderContext);

private:
    pimpl::Pimpl<class VulkanRenderImpl, 1024> m_impl;
};
