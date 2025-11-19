#pragma once
#include "Internal/Tex3D.h"


namespace Simple3D {
    TextureCube::TextureCube(Device& dev,
        const std::vector<std::string>& facePaths,
        bool generateMipmapsEnabled, VkCommandPool* CommandPool)
        : device(&dev), CommandPool(CommandPool)
    {
        if (facePaths.size() != 6) {
            throw std::runtime_error("TextureCube requires exactly 6 face paths!");
        }

        std::vector<unsigned char*> facesData;
        LoadFaces(facePaths, facesData);

        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

        if (generateMipmapsEnabled) {
            mipLevels = static_cast<uint32_t>(
                std::floor(std::log2(std::max(width, height)))) + 1;
        }

        CreateImageCube(format);
        UploadToGPU(facesData, format);

        if (generateMipmapsEnabled)
            GenerateMipmaps(format);

        CreateImageViewCube(format);
        CreateSampler();

        // Prepare descriptor info for pipelines
        descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        descriptor.imageView = imageView;
        descriptor.sampler = sampler;

        for (auto ptr : facesData)
            stbi_image_free(ptr);
    }

    TextureCube::~TextureCube() {
        Destroy();
    }

    void TextureCube::Destroy() {
        auto dev = device->getLogicalDevice();

        if (sampler) vkDestroySampler(dev, sampler, nullptr);
        if (imageView) vkDestroyImageView(dev, imageView, nullptr);
        if (image) vkDestroyImage(dev, image, nullptr);
        if (memory) vkFreeMemory(dev, memory, nullptr);
    }


    // ------------------------------
    // load 6 faces into memory
    // ------------------------------
    void TextureCube::LoadFaces(const std::vector<std::string>& faces,
        std::vector<unsigned char*>& pixels)
    {
        int w, h, comp;

        for (const auto& path : faces) {
            unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
            if (!data)
                throw std::runtime_error("Failed to load cubemap face: " + path);

            if (pixels.empty()) {
                width = w;
                height = h;
            }
            else {
                if (w != (int)width || h != (int)height) {
                    throw std::runtime_error("Cubemap faces have mismatched dimensions!");
                }
            }

            pixels.push_back(data);
        }
    }

    // ------------------------------
    // Create the GPU image (6 layers)
    // ------------------------------
    void TextureCube::CreateImageCube(VkFormat format)
    {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.extent.width = width;
        info.extent.height = height;
        info.extent.depth = 1;

        info.arrayLayers = 6;
        info.mipLevels = mipLevels;

        info.format = format;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        createImage(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory, device);
    }

    // ------------------------------
    // Upload the 6 images to GPU layers
    // ------------------------------
    void TextureCube::UploadToGPU(std::vector<unsigned char*>& pixelData, VkFormat format)
    {
        VkDeviceSize layerSize = width * height * 4;
        VkDeviceSize totalSize = layerSize * 6;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        createBuffer(totalSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingMemory,
            device);

        // copy all faces into staging memory
        unsigned char* mapped;
        vkMapMemory(device->getLogicalDevice(), stagingMemory, 0, totalSize, 0, (void**)&mapped);

        for (int i = 0; i < 6; i++) {
            memcpy(mapped + layerSize * i, pixelData[i], layerSize);
        }

        vkUnmapMemory(device->getLogicalDevice(), stagingMemory);

        transitionImageLayout(image, format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            mipLevels, 6, device, CommandPool);

        VkCommandBuffer cmd = beginSingleTimeCommands(device, CommandPool);

        std::array<VkBufferImageCopy, 6> regions{};
        for (uint32_t face = 0; face < 6; face++) {
            regions[face].bufferOffset = layerSize * face;
            regions[face].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            regions[face].imageSubresource.mipLevel = 0;
            regions[face].imageSubresource.baseArrayLayer = face;
            regions[face].imageSubresource.layerCount = 1;
            regions[face].imageExtent = { width, height, 1 };
        }

        vkCmdCopyBufferToImage(
            cmd,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            6,
            regions.data()
        );

        endSingleTimeCommands(device, CommandPool, &cmd);

        vkDestroyBuffer(device->getLogicalDevice(), stagingBuffer, nullptr);
        vkFreeMemory(device->getLogicalDevice(), stagingMemory, nullptr);
    }

    // ------------------------------
    // GPU mipmap generation
    // ------------------------------
    void TextureCube::GenerateMipmaps(VkFormat format)
    {
        // Use your existing mipmap generator OR I can generate one
        generateMipmaps(image, format, width, height, mipLevels, 6, device, CommandPool);
    }

    // ------------------------------
    // Create cube image view
    // ------------------------------
    void TextureCube::CreateImageViewCube(VkFormat format)
    {
        imageView = createImageView(
            image, format, VK_IMAGE_ASPECT_COLOR_BIT,
            mipLevels, 6, VK_IMAGE_VIEW_TYPE_CUBE, device
        );
    }

    // ------------------------------
    // Create sampler
    // ------------------------------
    void TextureCube::CreateSampler()
    {
        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.maxAnisotropy = 16;
        info.anisotropyEnable = VK_TRUE;
        info.minLod = 0.0f;
        info.maxLod = (float)mipLevels;

        if (vkCreateSampler(device->getLogicalDevice(), &info, nullptr, &sampler) != VK_SUCCESS)
            throw std::runtime_error("Failed to create cubemap sampler!");
    }
}