#define STB_IMAGE_IMPLEMENTATION
#include "Component/Tools/Material.h"
#include <vendor/Stb/stb_image.h>

namespace Simple3D {

    Material::Material(Device* device, VkCommandPool* commandPool, std::string vertex, std::string Fragment, std::unordered_map<std::string, std::string> texture_files, bool isLit)
        : r_device(device), r_commandPool(commandPool)
        , vertexSource(vertex), FragmentSource(Fragment), isLit(isLit)
    {
        // Create a TextureBinding for each texture name
        for (const auto& name : texture_files) {
            textures[name.first] = CreateTexture(name.second);
        }
    }

    Material::~Material() {
        // Clean up all textures
        for (const auto& pair : textures) {
            DestroyImage(pair.second);
        }
    }

    void Material::updateSortedTextureNames() {
        sortedTextureNames.clear();
        for (const auto& [name, _] : textures) {
            sortedTextureNames.push_back(name);
        }
        std::sort(sortedTextureNames.begin(), sortedTextureNames.end());
    }

    // Get texture index by name
    int Material::getTextureIndex(const std::string& name) const {
        auto it = std::find(sortedTextureNames.begin(), sortedTextureNames.end(), name);
        if (it == sortedTextureNames.end()) {
            throw std::runtime_error("Texture not found: " + name);
        }
        return static_cast<int>(it - sortedTextureNames.begin());
    }


    TextureBinding Material::CreateTexture(std::string texture) {
        TextureBinding binding = {};


        // Load image into GPU memory
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(texture.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, r_device);

        void* data;
        vkMapMemory(r_device->getLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(r_device->getLogicalDevice(), stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, binding.textureImage, binding.textureImageMemory, r_device);

        transitionImageLayout(binding.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, r_device, r_commandPool);
        copyBufferToImage(stagingBuffer, binding.textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), r_device, r_commandPool);

        transitionImageLayout(binding.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, r_device, r_commandPool);

        //transitionImageLayout(binding.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, r_device, r_commandPool);

        vkDestroyBuffer(r_device->getLogicalDevice(), stagingBuffer, nullptr);
        vkFreeMemory(r_device->getLogicalDevice(), stagingBufferMemory, nullptr);


        // Create image view and sampler
        binding.view = createImageView(binding.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, r_device);
        createTextureSampler(&binding, r_device);



        return binding;
    }

    void Material::DestroyImage(TextureBinding binding) {
        vkDestroySampler(r_device->getLogicalDevice(), binding.sampler, nullptr);
        vkDestroyImageView(r_device->getLogicalDevice(), binding.view, nullptr);
        vkFreeMemory(r_device->getLogicalDevice(), binding.textureImageMemory, nullptr);
        vkDestroyImage(r_device->getLogicalDevice(), binding.textureImage, nullptr);
    }
}