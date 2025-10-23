#pragma once
#include "SimpleCore.h"
#include "Internal/Material/ShaderSet.h"
#include "RenderTarget.h"
#include "Device.h"
#include "Tools.h"

namespace Simple3D {


    struct BindingInfo {
        std::string name;
        uint32_t binding = 0;
        uint32_t set = 0;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uint32_t count = 1;
        VkShaderStageFlags stageFlags = 0;

        // Per-frame buffers (only used for UBO/SSBO types)
        std::vector<VkBuffer> buffers;                      // size == MAX_FRAMES_IN_FLIGHT
        std::vector<VkDeviceMemory> bufferMemories;         // size == MAX_FRAMES_IN_FLIGHT
        std::vector<void*> mappedData;                      
        size_t dataSize = 0;

        // For images
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };

    class Pipeline {
    public:
        Pipeline(Device& device, ShaderSet& shaderSet, VkRenderPass renderPass, RenderTarget& target);
        ~Pipeline();

        VkPipeline GetPipeline() const { return pipeline; };
        VkPipelineLayout GetLayout() const { return layout; };

        // Bind arbitrary data (UBO, texture, storage buffer) by descriptor name
        void BindData(const std::string& name, const void* data, size_t size, uint32_t arrayIndex = 0);
        void BindData(const std::string& name, const VkDescriptorImageInfo& imgInfo, uint32_t arrayIndex = 0);

        // Update GPU buffers, call vkUpdateDescriptorSets internally
        void UpdateDescriptors(uint32_t frameIndex);

        // Returns all descriptor sets for a given frame index
        inline const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const {
            return descriptorSets[frameIndex];
        }

        // For convenience if you only need the first set (like old code)
        inline VkDescriptorSet GetFirstDescriptorSet(uint32_t frameIndex) const {
            return descriptorSets[frameIndex].empty() ? VK_NULL_HANDLE : descriptorSets[frameIndex][0];
        }

    private:
        ShaderSet* shaderSet;
        Device* device;
        RenderTarget& target;

        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;

        std::vector<VkDescriptorSetLayout> setLayouts;
        std::vector<std::vector<VkDescriptorSet>> descriptorSets;

        std::unordered_map<std::string, BindingInfo> bindings;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;


        void CreatePipeline(VkRenderPass renderPass);
        void CreateDescriptorSets();
        void CreateDescriptorPool();
        void PopulateBindingsFromShaderSet();
    };
}