#include "Internal/Tools.h"
#include "Internal/Allocator.h"
#include <vendor/Stb/stb_image.h>
#include <stdexcept>
#include <array>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace Simple3D {

    // ── createBuffer ─────────────────────────────────────────────────────────────
    void createBuffer(VkDeviceSize             size,
        VkBufferUsageFlags       usage,
        VmaMemoryUsage           memUsage,
        VmaAllocationCreateFlags allocFlags,
        VkBuffer& buffer,
        Allocation& alloc,
        Device* device)
    {
        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size = size;
        bci.usage = usage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo aci{};
        aci.usage = memUsage;
        aci.flags = allocFlags;

        if (vmaCreateBuffer(Allocator::Get(), &bci, &aci,
            &buffer, &alloc.handle, &alloc.info) != VK_SUCCESS)
            throw std::runtime_error("createBuffer: vmaCreateBuffer failed");
    }

    // ── createImage ───────────────────────────────────────────────────────────────
    void createImage(const VkImageCreateInfo& info,
        VkImage& image,
        Allocation& alloc,
        Device* device)
    {
        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (vmaCreateImage(Allocator::Get(), &info, &aci,
            &image, &alloc.handle, &alloc.info) != VK_SUCCESS)
            throw std::runtime_error("createImage: vmaCreateImage failed");
    }

    // ── Single-time commands ──────────────────────────────────────────────────────
    VkCommandBuffer beginSingleTimeCommands(Device* device, VkCommandPool* pool)
    {
        VkCommandBufferAllocateInfo alloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandPool = *pool;
        alloc.commandBufferCount = 1;

        VkCommandBuffer cmd;
        if (vkAllocateCommandBuffers(device->getLogicalDevice(), &alloc, &cmd) != VK_SUCCESS)
            throw std::runtime_error("beginSingleTimeCommands: failed to allocate command buffer");

        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);

        return cmd;
    }

    void endSingleTimeCommands(Device* device,
        VkCommandPool* pool,
        VkCommandBuffer* cmd)
    {
        vkEndCommandBuffer(*cmd);

        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = cmd;

        VkQueue queue = device->getVKgraphicsQueue();
        vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device->getLogicalDevice(), *pool, 1, cmd);
    }

    // ── generateMipmaps ──────────────────────────────────────────────────────────
    void generateMipmaps(VkImage        image,
        VkFormat       format,
        int32_t        texWidth,
        int32_t        texHeight,
        uint32_t       mipLevels,
        uint32_t       layers,
        Device* device,
        VkCommandPool* pool)
    {
        // Check that the format supports linear blitting
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &props);
        if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
            throw std::runtime_error("generateMipmaps: image format does not support linear blitting");

        VkCommandBuffer cmd = beginSingleTimeCommands(device, pool);

        VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = layers;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipW = texWidth;
        int32_t mipH = texHeight;

        for (uint32_t i = 1; i < mipLevels; ++i) {
            // Transition level i-1 to TRANSFER_SRC
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);

            int32_t nextW = std::max(mipW / 2, 1);
            int32_t nextH = std::max(mipH / 2, 1);

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipW, mipH, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = layers;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { nextW, nextH, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = layers;

            vkCmdBlitImage(cmd,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);

            // Transition level i-1 to SHADER_READ_ONLY
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);

            mipW = nextW;
            mipH = nextH;
        }

        // Transition the last mip level to SHADER_READ_ONLY
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(device, pool, &cmd);
    }

    // ── createImageView ───────────────────────────────────────────────────────────
    VkImageView createImageView(VkImage            image,
        VkFormat           format,
        VkImageAspectFlags aspectFlags,
        Device* device)
    {
        VkImageViewCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        info.image = image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = format;
        info.subresourceRange.aspectMask = aspectFlags;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        VkImageView view{};
        if (vkCreateImageView(device->getLogicalDevice(), &info, nullptr, &view) != VK_SUCCESS)
            throw std::runtime_error("createImageView: failed to create image view");
        return view;
    }

    VkImageView createImageView(VkImage            image,
        VkFormat           format,
        VkImageAspectFlags aspectFlags,
        uint32_t           mipLevels,
        uint32_t           layers,
        VkImageViewType    viewType,
        Device* device)
    {
        VkImageViewCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        info.image = image;
        info.viewType = viewType;
        info.format = format;
        info.subresourceRange.aspectMask = aspectFlags;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = mipLevels;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = layers;

        VkImageView view{};
        if (vkCreateImageView(device->getLogicalDevice(), &info, nullptr, &view) != VK_SUCCESS)
            throw std::runtime_error("createImageView: failed to create image view (extended)");
        return view;
    }

    // ── copyBuffer ────────────────────────────────────────────────────────────────
    void copyBuffer(VkBuffer       src,
        VkBuffer       dst,
        VkDeviceSize   size,
        Device* device,
        VkCommandPool* pool)
    {
        VkCommandBuffer cmd = beginSingleTimeCommands(device, pool);
        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmd, src, dst, 1, &region);
        endSingleTimeCommands(device, pool, &cmd);
    }

    // ── copyBufferToImage ─────────────────────────────────────────────────────────
    void copyBufferToImage(VkBuffer       buffer,
        VkImage        image,
        uint32_t       width,
        uint32_t       height,
        Device* device,
        VkCommandPool* pool)
    {
        copyBufferToImage(buffer, image, width, height, 1, device, pool);
    }

    void copyBufferToImage(VkBuffer       buffer,
        VkImage        image,
        uint32_t       width,
        uint32_t       height,
        uint32_t       layers,
        Device* device,
        VkCommandPool* pool)
    {
        VkCommandBuffer cmd = beginSingleTimeCommands(device, pool);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = layers;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(cmd, buffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(device, pool, &cmd);
    }

    // ── transitionImageLayout ─────────────────────────────────────────────────────
    static void doTransition(VkCommandBuffer cmd,
        VkImage         image,
        VkFormat        format,
        VkImageLayout   oldLayout,
        VkImageLayout   newLayout,
        uint32_t        mipLevels,
        uint32_t        layerCount)
    {
        VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                format == VK_FORMAT_D24_UNORM_S8_UINT)
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = layerCount;

        VkPipelineStageFlags srcStage{}, dstStage{};

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else {
            // Generic safe fallback
            barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void transitionImageLayout(VkImage        image,
        VkFormat       format,
        VkImageLayout  oldLayout,
        VkImageLayout  newLayout,
        Device* device,
        VkCommandPool* pool,
        uint32_t       layerCount)
    {
        VkCommandBuffer cmd = beginSingleTimeCommands(device, pool);
        doTransition(cmd, image, format, oldLayout, newLayout, 1, layerCount);
        endSingleTimeCommands(device, pool, &cmd);
    }

    void transitionImageLayout(VkImage        image,
        VkFormat       format,
        VkImageLayout  oldLayout,
        VkImageLayout  newLayout,
        uint32_t       mipLevels,
        uint32_t       layers,
        Device* device,
        VkCommandPool* pool)
    {
        VkCommandBuffer cmd = beginSingleTimeCommands(device, pool);
        doTransition(cmd, image, format, oldLayout, newLayout, mipLevels, layers);
        endSingleTimeCommands(device, pool, &cmd);
    }

    // ── findSupportedFormat / findDepthFormat / hasStencilComponent ──────────────
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
        VkImageTiling                tiling,
        VkFormatFeatureFlags         features,
        Device* device)
    {
        for (VkFormat fmt : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), fmt, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR &&
                (props.linearTilingFeatures & features) == features)
                return fmt;

            if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                (props.optimalTilingFeatures & features) == features)
                return fmt;
        }
        throw std::runtime_error("findSupportedFormat: no suitable format found");
    }

    VkFormat findDepthFormat(Device* device)
    {
        return findSupportedFormat(
            { VK_FORMAT_D32_SFLOAT,
              VK_FORMAT_D32_SFLOAT_S8_UINT,
              VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
            device);
    }

    bool hasStencilComponent(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    // ── findMemoryType ────────────────────────────────────────────────────────────
    uint32_t findMemoryType(uint32_t              typeFilter,
        VkMemoryPropertyFlags properties,
        Device* device)
    {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(device->getPhysicalDevice(), &memProps);

        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((typeFilter & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        throw std::runtime_error("findMemoryType: no suitable memory type found");
    }

    // ── createTextureSampler ──────────────────────────────────────────────────────
    void createTextureSampler(TextureBinding* binding, Device* device)
    {
        VkPhysicalDeviceProperties devProps{};
        vkGetPhysicalDeviceProperties(device->getPhysicalDevice(), &devProps);

        VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.anisotropyEnable = VK_TRUE;
        info.maxAnisotropy = devProps.limits.maxSamplerAnisotropy;
        info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        info.unnormalizedCoordinates = VK_FALSE;
        info.compareEnable = VK_FALSE;
        info.compareOp = VK_COMPARE_OP_ALWAYS;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.mipLodBias = 0.0f;
        info.minLod = 0.0f;
        info.maxLod = VK_LOD_CLAMP_NONE;

        if (vkCreateSampler(device->getLogicalDevice(), &info, nullptr, &binding->sampler) != VK_SUCCESS)
            throw std::runtime_error("createTextureSampler: failed to create sampler");
    }

    // ── CreateTextureBinding ──────────────────────────────────────────────────────
    TextureBinding CreateTextureBinding(const std::string& filepath,
        Device* device,
        VkCommandPool* pool)
    {
        // Load pixels
        int texW{}, texH{}, texChannels{};
        stbi_uc* pixels = stbi_load(filepath.c_str(), &texW, &texH,
            &texChannels, STBI_rgb_alpha);
        if (!pixels)
            throw std::runtime_error("CreateTextureBinding: failed to load image: " + filepath);

        VkDeviceSize imageSize = static_cast<VkDeviceSize>(texW) * texH * 4;

        // Staging buffer (host-visible, host-coherent)
        VkBuffer   stagingBuffer{};
        Allocation stagingAlloc{};
        createBuffer(imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            stagingBuffer, stagingAlloc, device);

        void* mapped{};
        vmaMapMemory(Allocator::Get(), stagingAlloc.handle, &mapped);
        std::memcpy(mapped, pixels, static_cast<size_t>(imageSize));
        vmaUnmapMemory(Allocator::Get(), stagingAlloc.handle);
        stbi_image_free(pixels);

        // Device-local image
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent = { static_cast<uint32_t>(texW), static_cast<uint32_t>(texH), 1 };
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.format = VK_FORMAT_R8G8B8A8_SRGB;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        TextureBinding binding{};
        binding.width = texW;
        binding.height = texH;

        VkImage    image{};
        Allocation imageAlloc{};
        createImage(ici, image, imageAlloc, device);
        binding.textureImage = image;
        binding.textureImageMemory = imageAlloc.info.deviceMemory;

        // Upload
        transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            device, pool);

        copyBufferToImage(stagingBuffer, image,
            static_cast<uint32_t>(texW),
            static_cast<uint32_t>(texH),
            device, pool);

        transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            device, pool);

        // Cleanup staging (VMA path — no legacy table entry needed)
        vmaDestroyBuffer(Allocator::Get(), stagingBuffer, stagingAlloc.handle);

        // View + sampler
        binding.view = createImageView(image, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_ASPECT_COLOR_BIT, device);
        createTextureSampler(&binding, device);

        return binding;
    }

} // namespace Simple3D