#include "Internal/Device.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace Simple3D {

    Device::Device(VkInstance& instance, VkSurfaceKHR& surface)
        : instance(instance), surface(surface)
    {
        PickPhysicalDevice();   // also calls probeCapabilities()
        createLogicalDevice();
    }

    Device::~Device() {
        vkDestroyDevice(device, nullptr);
    }

    // ── Physical device ───────────────────────────────────────────────────────────

    void Device::PickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0)
            throw std::runtime_error("Device: no GPU with Vulkan support found");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& dev : devices) {
            if (isDeviceSuitable(dev)) {
                physicalDevice = dev;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE)
            throw std::runtime_error("Device: no suitable GPU found");

        probeCapabilities();
    }

    bool Device::isDeviceSuitable(VkPhysicalDevice p_device) {
        QueueFamilyIndices indices = findQueueFamilies(p_device);
        if (!indices.isComplete()) return false;
        if (!checkDeviceExtensionSupport(p_device)) return false;

        SwapChainSupportDetails sc = querySwapChainSupport(p_device);
        return !sc.formats.empty() && !sc.presentModes.empty();
    }

    bool Device::checkDeviceExtensionSupport(VkPhysicalDevice p_device) {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(p_device, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available(count);
        vkEnumerateDeviceExtensionProperties(p_device, nullptr, &count, available.data());

        std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& ext : available)
            required.erase(ext.extensionName);
        return required.empty();
    }

    // ── Capability probing ────────────────────────────────────────────────────────
    // Queries all optional feature bits from the physical device BEFORE we create
    // the logical device, so createLogicalDevice knows what to request.

    void Device::probeCapabilities() {
        // ── Extension presence ────────────────────────────────────────────────
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, exts.data());

        auto hasExt = [&](const char* name) {
            return std::any_of(exts.begin(), exts.end(),
                [name](const VkExtensionProperties& e) {
                    return std::strcmp(e.extensionName, name) == 0;
                });
            };

        bool hasAS = hasExt(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        bool hasRQ = hasExt(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        bool hasDHO = hasExt(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        bool hasBDA = hasExt(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        bool hasRTP = hasExt(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

        // RT requires all four base extensions.
        bool rtExtPresent = hasAS && hasRQ && hasDHO && hasBDA;

        // ── Feature bits ──────────────────────────────────────────────────────
        // Chain together all structures we want to query at once.
        VkPhysicalDeviceBufferDeviceAddressFeatures bdaF{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR asF{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        asF.pNext = &bdaF;

        VkPhysicalDeviceRayQueryFeaturesKHR rqF{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
        rqF.pNext = &asF;

        VkPhysicalDeviceDescriptorIndexingFeatures diF{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
        diF.pNext = &rqF;

        VkPhysicalDeviceVulkan12Features vk12F{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        vk12F.pNext = &diF;

        VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        features2.pNext = &vk12F;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

        // ── Populate caps ─────────────────────────────────────────────────────
        m_Caps.bufferDeviceAddress = rtExtPresent && bdaF.bufferDeviceAddress;
        m_Caps.accelerationStructure = rtExtPresent && asF.accelerationStructure;
        m_Caps.rayQuery = rtExtPresent && rqF.rayQuery;
        m_Caps.rayTracingPipeline = hasRTP &&
            m_Caps.accelerationStructure; // RTP needs AS
        m_Caps.descriptorIndexing = diF.runtimeDescriptorArray &&
            diF.descriptorBindingPartiallyBound &&
            diF.descriptorBindingSampledImageUpdateAfterBind;
        m_Caps.shaderOutputLayer = vk12F.shaderOutputLayer;
    }

    // ── Logical device ────────────────────────────────────────────────────────────

    void Device::createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::set<uint32_t> uniqueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };

        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCIs;
        for (uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo qi{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO }; // was DEVICE_CREATE_INFO — typo fix
            qi.queueFamilyIndex = family;
            qi.queueCount = 1;
            qi.pQueuePriorities = &priority;
            queueCIs.push_back(qi);
        }

        // ── Collect enabled device extensions ────────────────────────────────
        std::vector<const char*> enabledExts(deviceExtensions.begin(),
            deviceExtensions.end());

        // Buffer device address is needed by VMA's RT flag and by AS builds.
        if (m_Caps.bufferDeviceAddress)
            enabledExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

        if (m_Caps.accelerationStructure) {
            enabledExts.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            enabledExts.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        }
        if (m_Caps.rayQuery)
            enabledExts.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);

        if (m_Caps.rayTracingPipeline)
            enabledExts.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

        // ── Basic features ────────────────────────────────────────────────────
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;

        // ── pNext chain — built bottom-up so each struct points at the next ──
        // Start with a null tail and prepend each optional block.
        void* pNextTail = nullptr;

        // Vulkan 1.2 core features (gl_Layer, shaderOutputViewportIndex).
        // Always requested — all Vulkan 1.2 devices support these if shaderOutputLayer
        // was reported true; we still fill the struct even when false so the chain
        // is valid (Vulkan ignores false flags).
        static VkPhysicalDeviceVulkan12Features vk12F{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        vk12F = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        vk12F.shaderOutputLayer = m_Caps.shaderOutputLayer ? VK_TRUE : VK_FALSE;
        vk12F.shaderOutputViewportIndex = m_Caps.shaderOutputLayer ? VK_TRUE : VK_FALSE;
        // BDA feature bit lives here in Vulkan 1.2 as well as the separate struct.
        vk12F.bufferDeviceAddress = m_Caps.bufferDeviceAddress ? VK_TRUE : VK_FALSE;
        vk12F.pNext = pNextTail;
        pNextTail = &vk12F;

        // Descriptor indexing — needed for bindless textures.
        static VkPhysicalDeviceDescriptorIndexingFeatures diF{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
        if (m_Caps.descriptorIndexing) {
            diF = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
            diF.runtimeDescriptorArray = VK_TRUE;
            diF.descriptorBindingPartiallyBound = VK_TRUE;
            diF.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
            diF.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
            diF.pNext = pNextTail;
            pNextTail = &diF;
        }

        // Buffer device address — also needed by VMA RT flag.
        static VkPhysicalDeviceBufferDeviceAddressFeatures bdaF{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
        if (m_Caps.bufferDeviceAddress) {
            bdaF = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
            bdaF.bufferDeviceAddress = VK_TRUE;
            bdaF.pNext = pNextTail;
            pNextTail = &bdaF;
        }

        // Acceleration structure.
        static VkPhysicalDeviceAccelerationStructureFeaturesKHR asF{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        if (m_Caps.accelerationStructure) {
            asF = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
            asF.accelerationStructure = VK_TRUE;
            asF.pNext = pNextTail;
            pNextTail = &asF;
        }

        // Ray query (fragment shader trace).
        static VkPhysicalDeviceRayQueryFeaturesKHR rqF{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
        if (m_Caps.rayQuery) {
            rqF = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
            rqF.rayQuery = VK_TRUE;
            rqF.pNext = pNextTail;
            pNextTail = &rqF;
        }

        // Ray tracing pipeline (optional full RT pipeline).
        static VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtpF{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        if (m_Caps.rayTracingPipeline) {
            rtpF = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
            rtpF.rayTracingPipeline = VK_TRUE;
            rtpF.pNext = pNextTail;
            pNextTail = &rtpF;
        }

        // ── Create the logical device ─────────────────────────────────────────
        VkDeviceCreateInfo ci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        ci.pNext = pNextTail;
        ci.queueCreateInfoCount = static_cast<uint32_t>(queueCIs.size());
        ci.pQueueCreateInfos = queueCIs.data();
        ci.pEnabledFeatures = &deviceFeatures;
        ci.enabledExtensionCount = static_cast<uint32_t>(enabledExts.size());
        ci.ppEnabledExtensionNames = enabledExts.data();

        if (vkCreateDevice(physicalDevice, &ci, nullptr, &device) != VK_SUCCESS)
            throw std::runtime_error("Device: failed to create logical device");

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);  // was graphicsQueue — bug fix
    }

    // ── Queue family / swap chain queries ────────────────────────────────────────

    QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice p_device) {
        QueueFamilyIndices indices{};
        if (p_device == VK_NULL_HANDLE) return indices;

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(p_device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(p_device, &count, families.data());

        for (uint32_t i = 0; i < count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.graphicsFamily = i;

            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface, &present);
            if (present) indices.presentFamily = i;

            if (indices.isComplete()) break;
        }
        return indices;
    }

    QueueFamilyIndices Device::findQueueFamilies() {
        return findQueueFamilies(physicalDevice);
    }

    SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice p_device) {
        SwapChainSupportDetails d;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_device, surface, &d.capabilities);

        uint32_t fmt = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &fmt, nullptr);
        if (fmt) { d.formats.resize(fmt); vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &fmt, d.formats.data()); }

        uint32_t pm = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &pm, nullptr);
        if (pm) { d.presentModes.resize(pm); vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &pm, d.presentModes.data()); }

        return d;
    }

    SwapChainSupportDetails Device::querySwapChainSupport() {
        return querySwapChainSupport(physicalDevice);
    }

    // ── Accessors ─────────────────────────────────────────────────────────────────

    VkDevice            Device::getLogicalDevice() { return device; }
    VkPhysicalDevice    Device::getPhysicalDevice() { return physicalDevice; }
    VkQueue             Device::getVKgraphicsQueue() { return graphicsQueue; }
    VkQueue             Device::getVKpresentQueue() { return presentQueue; }  // was graphicsQueue — bug fix

    VkPhysicalDeviceProperties Device::GetProperties() {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(physicalDevice, &p);
        return p;
    }

    uint32_t Device::findMemoryType(uint32_t typeFilter,
        VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties mem{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mem);
        for (uint32_t i = 0; i < mem.memoryTypeCount; ++i)
            if ((typeFilter & (1u << i)) &&
                (mem.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        throw std::runtime_error("Device: no suitable memory type");
    }

} // namespace Simple3D