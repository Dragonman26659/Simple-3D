#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"
#include "Internal/Allocator.h"   // brings in Allocation + VmaAllocator

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace Simple3D {

    // ── TextureBinding ────────────────────────────────────────────────────────
    struct TextureBinding {
        VkImage         textureImage = VK_NULL_HANDLE;
        VkDeviceMemory  textureImageMemory = VK_NULL_HANDLE;
        VkImageView     view = VK_NULL_HANDLE;
        VkSampler       sampler = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        int             width = 0;
        int             height = 0;

        // Set to a valid index after BindlessRegistry::Register().
        // UINT32_MAX means "not registered in the bindless table".
        uint32_t        bindlessHandle = UINT32_MAX;
    };

    // =========================================================================
    //  Core VMA-backed helpers (non-inline, defined in Tools.cpp)
    // =========================================================================

    // Buffer allocation via VMA.
    void createBuffer(VkDeviceSize            size,
        VkBufferUsageFlags      usage,
        VmaMemoryUsage          memUsage,
        VmaAllocationCreateFlags allocFlags,
        VkBuffer& buffer,
        Allocation& alloc,
        Device* device);

    // Image allocation via VMA.
    void createImage(const VkImageCreateInfo& info,
        VkImage& image,
        Allocation& alloc,
        Device* device);

    // Single-time command helpers.
    VkCommandBuffer beginSingleTimeCommands(Device* device,
        VkCommandPool* pool);
    void            endSingleTimeCommands(Device* device,
        VkCommandPool* pool,
        VkCommandBuffer* cmd);

    // Mipmap generation (blit-based, requires linear-filter support).
    void generateMipmaps(VkImage        image,
        VkFormat       format,
        int32_t        texWidth,
        int32_t        texHeight,
        uint32_t       mipLevels,
        uint32_t       layers,
        Device* device,
        VkCommandPool* pool);

    // Image views.
    VkImageView createImageView(VkImage            image,
        VkFormat           format,
        VkImageAspectFlags aspectFlags,
        Device* device);

    VkImageView createImageView(VkImage            image,
        VkFormat           format,
        VkImageAspectFlags aspectFlags,
        uint32_t           mipLevels,
        uint32_t           layers,
        VkImageViewType    viewType,
        Device* device);

    // Buffer copy.
    void copyBuffer(VkBuffer       src,
        VkBuffer       dst,
        VkDeviceSize   size,
        Device* device,
        VkCommandPool* pool);

    // Image copies.
    void copyBufferToImage(VkBuffer       buffer,
        VkImage        image,
        uint32_t       width,
        uint32_t       height,
        Device* device,
        VkCommandPool* pool);

    void copyBufferToImage(VkBuffer       buffer,
        VkImage        image,
        uint32_t       width,
        uint32_t       height,
        uint32_t       layers,
        Device* device,
        VkCommandPool* pool);

    // Layout transitions.
    // 6-arg: 1 mip level, N layers (default 1).
    void transitionImageLayout(VkImage        image,
        VkFormat       format,
        VkImageLayout  oldLayout,
        VkImageLayout  newLayout,
        Device* device,
        VkCommandPool* pool,
        uint32_t       layerCount = 1);

    // 8-arg: explicit mip + layer counts (used by TextureCube, DepthBuffer array).
    void transitionImageLayout(VkImage        image,
        VkFormat       format,
        VkImageLayout  oldLayout,
        VkImageLayout  newLayout,
        uint32_t       mipLevels,
        uint32_t       layers,
        Device* device,
        VkCommandPool* pool);

    // Format / memory queries.
    VkFormat  findSupportedFormat(const std::vector<VkFormat>& candidates,
        VkImageTiling                tiling,
        VkFormatFeatureFlags         features,
        Device* device);
    VkFormat  findDepthFormat(Device* device);
    bool      hasStencilComponent(VkFormat format);
    uint32_t  findMemoryType(uint32_t              typeFilter,
        VkMemoryPropertyFlags properties,
        Device* device);

    // Sampler + high-level texture loader.
    void           createTextureSampler(TextureBinding* binding, Device* device);
    TextureBinding CreateTextureBinding(const std::string& filepath,
        Device* device,
        VkCommandPool* pool);

    // =========================================================================
    //  Legacy-compatibility inline overloads
    //  (keep old call sites in Model, Material, Pipeline, DepthBuffer building
    //   without any source changes at those call sites)
    // =========================================================================

    // createBuffer — old signature with VkMemoryPropertyFlags + raw VkDeviceMemory.
    inline void createBuffer(VkDeviceSize          size,
        VkBufferUsageFlags    usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& outMemory,
        Device* device)
    {
        VmaMemoryUsage          memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VmaAllocationCreateFlags allocFlags = 0;

        if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            memUsage = VMA_MEMORY_USAGE_AUTO;
            allocFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }
        if (properties & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
            memUsage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;

        Allocation alloc;
        createBuffer(size, usage, memUsage, allocFlags, buffer, alloc, device);

        outMemory = alloc.info.deviceMemory;

        // Store in the legacy table so DestroyLegacyBuffer works correctly.
        Allocator::RegisterLegacyBuffer(buffer, alloc);
    }

    // createImage — old 9/10-arg signature with VkMemoryPropertyFlags + raw VkDeviceMemory.
    inline void createImage(uint32_t              width,
        uint32_t              height,
        VkFormat              format,
        VkImageTiling         tiling,
        VkImageUsageFlags     usage,
        VkMemoryPropertyFlags /*properties*/,   // ignored — VMA decides
        VkImage& image,
        VkDeviceMemory& outMemory,
        Device* device,
        uint32_t              arrayLayers = 1)
    {
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent = { width, height, 1 };
        ici.mipLevels = 1;
        ici.arrayLayers = arrayLayers;
        ici.format = format;
        ici.tiling = tiling;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.usage = usage;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        Allocation alloc;
        createImage(ici, image, alloc, device);

        outMemory = alloc.info.deviceMemory;
        Allocator::RegisterLegacyImage(image, alloc);
    }

    // =========================================================================
    //  Debug naming (no-op in release / when extension is absent)
    // =========================================================================
    inline void SetObjectName(VkDevice           device,
        uint64_t           handle,
        VkObjectType       type,
        const std::string& name)
    {
#ifdef VK_EXT_debug_utils
        static auto pfn = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));

        if (!pfn || name.empty()) return;

        VkDebugUtilsObjectNameInfoEXT info{
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name.c_str();
        pfn(device, &info);
#endif
    }

} // namespace Simple3D