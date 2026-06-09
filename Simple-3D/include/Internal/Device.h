#pragma once
#include "SimpleCore.h"
#include <optional>
#include <vector>
#include <set>

#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// ODR fix: inline so the definition is the same across all TUs that include this header.
inline const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR          capabilities;
    std::vector<VkSurfaceFormatKHR>   formats;
    std::vector<VkPresentModeKHR>     presentModes;
};

// ── RT / bindless capability flags ───────────────────────────────────────────
// Queried once during PickPhysicalDevice and stored so Device::createLogicalDevice
// knows which feature structs to chain into vkCreateDevice.
struct DeviceCapabilities {
    bool accelerationStructure = false;
    bool rayQuery = false;
    bool rayTracingPipeline = false;
    bool bufferDeviceAddress = false;
    bool descriptorIndexing = false;  // runtimeDescriptorArray + partiallyBound
    bool shaderOutputLayer = false;  // gl_Layer in vertex shader

    bool SupportsRTFragQuery() const {
        return accelerationStructure && rayQuery && bufferDeviceAddress;
    }
};

namespace Simple3D {

    class Device {
    public:
        Device(VkInstance& instance, VkSurfaceKHR& surface);
        ~Device();

        SwapChainSupportDetails  querySwapChainSupport();
        QueueFamilyIndices       findQueueFamilies();

        VkDevice            getLogicalDevice();
        VkPhysicalDevice    getPhysicalDevice();
        VkQueue             getVKgraphicsQueue();
        VkQueue             getVKpresentQueue();

        VkPhysicalDeviceProperties GetProperties();
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

        // RT / bindless support flags — available after construction.
        const DeviceCapabilities& GetCapabilities() const { return m_Caps; }

    private:
        VkInstance& instance;
        VkSurfaceKHR& surface;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice         device = VK_NULL_HANDLE;
        VkQueue          graphicsQueue = VK_NULL_HANDLE;
        VkQueue          presentQueue = VK_NULL_HANDLE;

        DeviceCapabilities m_Caps;

        void PickPhysicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice p_device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice p_device);

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        QueueFamilyIndices      findQueueFamilies(VkPhysicalDevice device);

        void createLogicalDevice();
        void probeCapabilities();   // fills m_Caps from physicalDevice
    };

} // namespace Simple3D