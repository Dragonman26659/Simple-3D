#pragma once
#include "SimpleCore.h"
#include "Device.h"
#include "Tools.h"
#include <vendor/Stb/stb_image.h>


namespace Simple3D {
    class TextureCube {
    public:
        TextureCube(Device& dev,
            const std::vector<std::string>& facePaths,
            bool generateMipmapsEnabled, VkCommandPool* CommandPool);

        ~TextureCube();

        // For binding to the pipeline
        const VkDescriptorImageInfo& GetDescriptor() const { return descriptor; }

        VkImageView GetImageView() const { return imageView; }
        VkSampler GetSampler() const { return sampler; }

    private:
        Device* device;

        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 1;

        VkDescriptorImageInfo descriptor{}; // ready for Pipeline::BindData
        VkCommandPool* CommandPool;

    private:
        void LoadFaces(const std::vector<std::string>& faces, std::vector<unsigned char*>& pixels);
        void Destroy();

        void CreateImageCube(VkFormat format);
        void CreateImageViewCube(VkFormat format);
        void CreateSampler();
        void UploadToGPU(std::vector<unsigned char*>& pixelData, VkFormat format);
        void GenerateMipmaps(VkFormat format);
    };
}