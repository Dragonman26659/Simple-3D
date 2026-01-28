#define STB_IMAGE_IMPLEMENTATION
#include "Component/Tools/Material.h"
#include <vendor/Stb/stb_image.h>

namespace Simple3D {


    inline void NameTextureBinding(Device* r_device, const TextureBinding& binding, const std::string& textureName)
    {
        if (!r_device) return;
        VkDevice device = r_device->getLogicalDevice();
        if (!device) return;

        SetObjectName(device, reinterpret_cast<uint64_t>(binding.view),
            VK_OBJECT_TYPE_IMAGE_VIEW, textureName + " View");

        SetObjectName(device, reinterpret_cast<uint64_t>(binding.sampler),
            VK_OBJECT_TYPE_SAMPLER, textureName + " Sampler");

        SetObjectName(device, reinterpret_cast<uint64_t>(binding.textureImage),
            VK_OBJECT_TYPE_IMAGE, textureName + " Image");

        SetObjectName(device, reinterpret_cast<uint64_t>(binding.textureImageMemory),
            VK_OBJECT_TYPE_DEVICE_MEMORY, textureName + " Memory");

        SetObjectName(device, reinterpret_cast<uint64_t>(binding.descriptorSet),
            VK_OBJECT_TYPE_DESCRIPTOR_SET, textureName + " DescriptorSet");
    }

    Material::Material(Device* device, VkCommandPool* commandPool, std::unordered_map<std::string, std::string> texture_files, ShaderSet* shaders)
        : r_device(device), r_commandPool(commandPool), shaders(shaders)
    {
        // Create a TextureBinding for each texture name
        for (const auto& name : texture_files) {
            textures[name.first] = CreateTexture(name.second);
        }
    }

    Material::~Material()
    {
        // Clean up all textures
        for (auto tex : sortedTextureNames) {
            DestroyImage(textures[tex]);
        }

        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(r_device->getLogicalDevice(), descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
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
            printf("Failed to read file properly");
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

        transitionImageLayout(binding.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, r_device, r_commandPool, 1);
        copyBufferToImage(stagingBuffer, binding.textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), r_device, r_commandPool);

        transitionImageLayout(binding.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, r_device, r_commandPool, 1);

        //transitionImageLayout(binding.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, r_device, r_commandPool);

        vkDestroyBuffer(r_device->getLogicalDevice(), stagingBuffer, nullptr);
        vkFreeMemory(r_device->getLogicalDevice(), stagingBufferMemory, nullptr);


        // Create image view and sampler
        binding.view = createImageView(binding.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, r_device);
        createTextureSampler(&binding, r_device);


        //NameTextureBinding(r_device, binding, texture);




        return binding;
    }

    void Material::DestroyImage(TextureBinding binding) {
        vkDestroySampler(r_device->getLogicalDevice(), binding.sampler, nullptr);
        vkDestroyImageView(r_device->getLogicalDevice(), binding.view, nullptr);
        vkFreeMemory(r_device->getLogicalDevice(), binding.textureImageMemory, nullptr);
        vkDestroyImage(r_device->getLogicalDevice(), binding.textureImage, nullptr);
    }

    void Material::CreateDescriptorSets()
    {
        constexpr uint32_t MATERIAL_SET = 1;

        const auto& setLayouts = shaders->GetDescriptorSetLayouts();
        const auto& descriptorMap = shaders->GetDescriptorMap();

        VkDescriptorSetLayout foundLayout = VK_NULL_HANDLE;
        uint32_t current_map_index = 0;

        for (const auto& [set_index_in_map, bindings] : descriptorMap) {
            if (set_index_in_map == MATERIAL_SET) {
                if (current_map_index < setLayouts.size()) {
                    foundLayout = setLayouts[current_map_index];
                }
                break;
            }
            current_map_index++;
        }

        if (foundLayout == VK_NULL_HANDLE) {
            DescritorsNeeded = false;
            return;
        }

        descriptorSetLayout = foundLayout;
        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

        std::unordered_map<VkDescriptorType, uint32_t> typeCounts;

        auto it = descriptorMap.find(MATERIAL_SET);
        if (it == descriptorMap.end()) {
            throw std::runtime_error("Material set missing in descriptor map");
        }

        for (const auto& binding : it->second) {
            typeCounts[binding.type] += binding.count * MAX_FRAMES_IN_FLIGHT;
        }

        std::vector<VkDescriptorPoolSize> poolSizes;
        for (const auto& [type, count] : typeCounts) {
            poolSizes.push_back({ type, count });
        }

        VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

        if (vkCreateDescriptorPool(r_device->getLogicalDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool");
        }

        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

        VkDescriptorSetAllocateInfo alloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        alloc.descriptorPool = descriptorPool;
        alloc.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        alloc.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(r_device->getLogicalDevice(), &alloc, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets");
        }

        CreatedDescriptors = true;
        DescritorsNeeded = true;
    }

    void Material::UpdateDescriptors(uint32_t frameIndex)
    {
        if (!DescritorsNeeded)
            return;

        if (!CreatedDescriptors)
            CreateDescriptorSets();

        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorImageInfo> imageInfos;

        imageInfos.reserve(textures.size());
        writes.reserve(textures.size());

        for (const auto& [name, tex] : textures) {
            const DescriptorBinding* refl = shaders->GetBinding(name);

            if (!refl) continue;
            if (refl->set != 1) continue;
            if (refl->type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER &&
                refl->type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
                continue;
            }

            // Create the info
            VkDescriptorImageInfo img{};
            img.imageView = tex.view;
            img.sampler = tex.sampler;
            img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            imageInfos.push_back(img);

            VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet = descriptorSets[frameIndex];
            write.dstBinding = refl->binding;
            write.dstArrayElement = 0;
            write.descriptorType = refl->type;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfos.back();

            writes.push_back(write);
        }

        vkUpdateDescriptorSets(
            r_device->getLogicalDevice(),
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr
        );
    }
}