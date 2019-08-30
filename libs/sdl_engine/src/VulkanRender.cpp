#include "VulkanRender.h"
#include "core/Base.h"
#include <algorithm>
#include <fstream>
#include <optional>
#include <set>
#include <vulkan/vulkan.h>

class VulkanRenderImpl {
public:
    void Initialize(const VulkanRender::GetInstanceExtensionsFunc& getInstanceExtensions,
                    const VulkanRender::CreateSurfaceFunc& createSurface) {

        auto validationLayers = GetValidationLayers();
        if (!CheckValidationLayers(validationLayers)) {
            FAIL_MSG("Not all requested validation layers are supported");
        }

        m_instance = CreateInstance(getInstanceExtensions(), validationLayers);
        m_surface = createSurface(m_instance);
        m_physicalDevice = PickPhysicalDevice(m_instance, m_surface);

        const QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice, m_surface);
        m_device = CreateLogicalDevice(m_physicalDevice, indices, validationLayers);
        m_graphicsQueue = GetGraphicsQueueHandle(m_device, indices);
        m_presentQueue = GetPresentQueueHandle(m_device, indices);

        // TODO: Pass in window dimenions to Initialize
        m_swapchain =
            CreateSwapchain(m_device, m_physicalDevice, m_surface, m_swapchain.handle, 800, 600);

        m_swapchainImageViews = CreateSwapchainImageViews(m_device, m_swapchain);

        m_renderPass = CreateRenderPass(m_device, m_swapchain);
        m_pipelineLayout = CreateGraphicsPipeline(m_device, m_swapchain);
    }

    void Shutdown() {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        for (auto& imageView : m_swapchainImageViews) {
            vkDestroyImageView(m_device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(m_device, m_swapchain.handle, nullptr);
        vkDestroyDevice(m_device, nullptr);
        // TODO: destroy surface
        vkDestroyInstance(m_instance, nullptr);
        m_instance = {};
    }

    void ResetOverlay(const char* /*file*/) {}

    bool OnWindowResized(int windowWidth, int windowHeight) {
        // TODO: RAII type for auto cleanup
        auto newSwapchain = CreateSwapchain(m_device, m_physicalDevice, m_surface,
                                            m_swapchain.handle, windowWidth, windowHeight);
        if (m_swapchain.handle != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_device, m_swapchain.handle, nullptr);
        }
        m_swapchain = newSwapchain;

        // TODO: RAII type for auto cleanup
        for (auto& imageView : m_swapchainImageViews) {
            vkDestroyImageView(m_device, imageView, nullptr);
        }
        m_swapchainImageViews = CreateSwapchainImageViews(m_device, m_swapchain);

        return true;
    }

    void RenderScene(double /*frameTime*/, const RenderContext& /*renderContext*/) {}

private:
    static VkInstance CreateInstance(const std::vector<const char*>& extensions,
                                     const std::vector<const char*>& validationLayers) {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_1;
        // TODO: fill out the rest of appInfo

        VkInstanceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &appInfo;
        info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        info.ppEnabledLayerNames = validationLayers.empty() ? nullptr : validationLayers.data();

        VkInstance instance{};
        VkResult res = vkCreateInstance(&info, nullptr, &instance);
        if (res != VK_SUCCESS) {
            FAIL_MSG("Failed to create vkInstance");
        }

        return instance;
    }

    static std::vector<const char*> GetValidationLayers() {
        return {"VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_standard_validation",
                "VK_LAYER_LUNARG_parameter_validation"};
    }

    static std::vector<const char*> GetRequiredDeviceExtensions() {
        return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    }

    static bool CheckValidationLayers(const std::vector<const char*> layers) {
        uint32_t layerCount{};
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : layers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily; // VK_QUEUE_GRAPHICS_BIT
        std::optional<uint32_t> presentFamily;  // vkGetPhysicalDeviceSurfaceSupportKHR
        bool IsComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice,
                                                VkSurfaceKHR surface) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount{};
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                                 queueFamilies.data());

        uint32_t i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.IsComplete()) {
                break;
            }
            ++i;
        }

        return indices;
    }

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    static SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice physicalDevice,
                                                         VkSurfaceKHR surface) {
        SwapchainSupportDetails details{};

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

        uint32_t formatCount{};
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                                 details.formats.data());
        }

        uint32_t presentModeCount{};
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount,
                                                  nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount,
                                                      details.presentModes.data());
        }

        return details;
    }

    static VkSurfaceFormatKHR
    ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
        // Pick the first one that supports 8-bit BGRA format with sRGB color space
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        // Otherwise, just return the first one
        return formats[0];
    }

    static VkPresentModeKHR
    ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
        // Prefer triple-buffering (mailbox) mode for low latency with no tearing
        for (const auto& presentMode : presentModes) {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return presentMode;
            }
        }

        // FIFO is the only mode guaranteed to be available
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width,
                                       uint32_t height) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            // Clamp desired width/height between min/max extents allowed
            VkExtent2D actualExtent = {width, height};
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                            capabilities.maxImageExtent.width);
            actualExtent.height =
                std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                           capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    struct Swapchain {
        VkSwapchainKHR handle;
        std::vector<VkImage> images;
        VkFormat format;
        VkExtent2D extent;
    };

    static Swapchain CreateSwapchain(VkDevice device, VkPhysicalDevice physicalDevice,
                                     VkSurfaceKHR surface, VkSwapchainKHR oldSwapchain,
                                     uint32_t width, uint32_t height) {
        SwapchainSupportDetails swapChainSupport = QuerySwapchainSupport(physicalDevice, surface);

        VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities, width, height);

        // Create at least one more than minimum so that we don't have to wait on driver to acquire
        // the next image, but clamp to max (if there is a max)
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0) {
            imageCount = std::max(imageCount, swapChainSupport.capabilities.maxImageCount);
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1; // > 1 for stereoscopic 3D
        // We will render directly to images, so usage is color attachment. If rendering to separate
        // image first to perform post-processing operations, this could be set to
        // VK_IMAGE_USAGE_TRANSFER_DST_BIT and use a memory operation to transfer the image to the
        // swap chain image.
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
        const uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                               indices.presentFamily.value()};

        // We will render to images from the graphics family queue, and then these images will go to
        // the present family to be presented. There are 2 modes for handling swap chain images
        // across multiple queue families:
        // - Exlusive: image owned by one queue family at a time, and explicitly transferred to the
        // other. Offers best performance.
        // - Concurrent: image can be used by multiple queue families without explicit ownership
        // transfers.
        // If queue families differ, we'll just use concurrent to avoid dealing with ownership
        // transfers.
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;     // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // e.g. rotate or flip
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

        // Whether to alpha blend with other windows in windowing system (unusual, so go with
        // opaque)
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // Clip obscured color pixels for better performance (e.g. another window partially covers
        // ours)
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = oldSwapchain;

        VkSwapchainKHR swapchain{};
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            FAIL_MSG("Failed to create swap chain");
        }

        std::vector<VkImage> swapChainImages;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapChainImages.data());

        return {swapchain, move(swapChainImages), surfaceFormat.format, extent};
    }

    static std::vector<VkImageView> CreateSwapchainImageViews(VkDevice device,
                                                              Swapchain& swapchain) {
        std::vector<VkImageView> imageViews(swapchain.images.size());

        for (size_t i = 0; i < swapchain.images.size(); ++i) {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapchain.images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapchain.format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
                FAIL_MSG("Failed to create image views");
            }
        }

        return imageViews;
    }

    static VkPhysicalDevice PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        ASSERT_MSG(deviceCount > 0, "No physical devices found");
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (IsPhysicalDeviceSuitable(device, surface)) {
                return device;
                break;
            }
        }

        FAIL_MSG("Failed to find suitable device (GPU)");
        return {};
    }

    // TODO: internal
    static bool CheckDeviceExtensionsSupport(VkPhysicalDevice physicalDevice) {
        uint32_t extensionCount{};
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount,
                                             availableExtensions.data());

        auto deviceExtenions = GetRequiredDeviceExtensions();

        // Check if all required extensions are available
        std::set<std::string> requiredExtensions(deviceExtenions.begin(), deviceExtenions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        return requiredExtensions.empty();
    }

    // TODO: internal
    static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        // TODO: implement in terms of vkGetPhysicalDeviceProperties and
        // vkGetPhysicalDeviceFeatures to select the most suitable device, if any.

        // Does device support desired queue families?
        const QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
        if (!indices.IsComplete())
            return false;

        // Does device support all required extensions?
        if (!CheckDeviceExtensionsSupport(physicalDevice))
            return false;

        // Does device + surface support swap chain we need?
        SwapchainSupportDetails swapChainSupport = QuerySwapchainSupport(physicalDevice, surface);
        const bool swapChainAdequate =
            !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        if (!swapChainAdequate)
            return false;

        return true;
    }

    // TODO: internal
    static auto CreateDeviceQueueCreateInfos(const QueueFamilyIndices& indices) {
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                                  indices.presentFamily.value()};
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        return queueCreateInfos;
    }

    static VkDevice CreateLogicalDevice(VkPhysicalDevice physicalDevice,
                                        const QueueFamilyIndices& indices,
                                        const std::vector<const char*>& validationLayers) {
        auto queueCreateInfos = CreateDeviceQueueCreateInfos(indices);

        // Specify set of device features we want to use
        // TODO: For now, we don't need anything special, so leave it at defaults
        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;

        // Set device-specific extensions, like VK_KHR_swapchain
        auto deviceExtensions = GetRequiredDeviceExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames =
            deviceExtensions.empty() ? nullptr : deviceExtensions.data();

        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames =
            validationLayers.empty() ? nullptr : validationLayers.data();

        VkDevice device{};
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            FAIL_MSG("Failed to create logical device");
        }

        return device;
    }

    static VkQueue GetGraphicsQueueHandle(VkDevice device, const QueueFamilyIndices& indices) {
        VkQueue queue{};
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &queue);
        return queue;
    }

    static VkQueue GetPresentQueueHandle(VkDevice device, const QueueFamilyIndices& indices) {
        VkQueue queue{};
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &queue);
        return queue;
    }

    static std::vector<char> FileToVector(const char* filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    static VkShaderModule CreateShaderModule(VkDevice device, std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            FAIL_MSG("Failed to create shader module");
        }
        return shaderModule;
    }

    static VkRenderPass CreateRenderPass(VkDevice device, const Swapchain& swapchain) {
        // For now, we need just a single color attachment
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchain.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // Clear attachment to black
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // Store result in memory for presentation
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // Not using stencil
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // Don't care about initial contents of attachment before render pass
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // Contents to be used for presentation
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Reference the single color attachment from above for the single subpass
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        // Graphics subpass; can also be compute or raytracing
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // This binds to the "layout(location = 0) out vec4 outColor" in the fragment shader
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        VkRenderPass renderPass;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            FAIL_MSG("Failed to create render pass");
        }

        return renderPass;
    }

    static VkPipelineLayout CreateGraphicsPipeline(VkDevice device, const Swapchain& swapchain) {
        auto vertShaderCode = FileToVector("libs/sdl_engine/src/vk_shaders/vert.spv");
        auto fragShaderCode = FileToVector("libs/sdl_engine/src/vk_shaders/frag.spv");

        VkShaderModule vertShaderModule = CreateShaderModule(device, vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(device, fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain.extent.width;
        viewport.height = (float)swapchain.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain.extent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f;          // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;          // Optional
        multisampling.pSampleMask = nullptr;            // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE;      // Optional

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;             // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;             // Optional

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;            // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr;         // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0;    // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
            VK_SUCCESS) {
            FAIL_MSG("Failed to create pipeline layout");
        }
        return pipelineLayout;
    }

    VkInstance m_instance{};
    VkSurfaceKHR m_surface{};
    VkPhysicalDevice m_physicalDevice{};
    VkDevice m_device{};
    VkQueue m_graphicsQueue{};
    VkQueue m_presentQueue{};
    Swapchain m_swapchain{};
    std::vector<VkImageView> m_swapchainImageViews{};
    VkRenderPass m_renderPass{};
    VkPipelineLayout m_pipelineLayout{};
};

VulkanRender::VulkanRender() = default;
VulkanRender::~VulkanRender() = default;

void VulkanRender::Initialize(const GetInstanceExtensionsFunc& getInstanceExtensions,
                              const CreateSurfaceFunc& createSurface) {
    return m_impl->Initialize(getInstanceExtensions, createSurface);
}

void VulkanRender::Shutdown() {
    m_impl->Shutdown();
}

void VulkanRender::ResetOverlay(const char* file) {
    m_impl->ResetOverlay(file);
}

bool VulkanRender::OnWindowResized(int windowWidth, int windowHeight) {
    return m_impl->OnWindowResized(windowWidth, windowHeight);
}

void VulkanRender::RenderScene(double frameTime, const RenderContext& renderContext) {
    m_impl->RenderScene(frameTime, renderContext);
}
