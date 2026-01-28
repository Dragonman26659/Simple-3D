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

        // size == MAX_FRAMES_IN_FLIGHT
        // Map of the buffer using frameidex
        std::vector<VkBuffer> buffers;
        std::vector<VkDeviceMemory> bufferMemories;         
        std::vector<void*> mappedData;                      
        std::vector<VkDeviceSize> bufferSizes;
        size_t dataSize = 0;

        // For images
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };





    // Allows for configuring of the renderpipeline by renderpasses
    struct PipelineConfig {
        // Basic pipeline type
        enum class Type { Graphics, Compute, RayTracing } type = Type::Graphics;

        // Vertex input
        bool useVertexInput = true;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Rasterizer
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        float lineWidth = 1.0f;
        bool depthClampEnable = VK_FALSE;
        bool rasterizerDiscardEnable = VK_FALSE;

        // Depth / stencil
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

        // Multisample
        VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        bool sampleShadingEnable = VK_FALSE;

        // Color blend (per-attachment options)
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;

        // Dynamic states (viewport/scissor are included by default)
        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        // Subpass index
        uint32_t subpass = 0;

        // Any extra flags for specialized pipelines
        // (e.g., enable alpha-to-coverage, conservative rasterization if supported)
        VkPipelineRasterizationStateCreateFlags rasterizerFlags = 0;

        // Convenience factory that fills default blend attachment for N attachments
        static PipelineConfig MakeDefaultForAttachments(uint32_t attachmentCount) {
            PipelineConfig cfg;
            cfg.blendAttachments.resize(attachmentCount);
            for (auto& a : cfg.blendAttachments) {
                a.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                a.blendEnable = VK_FALSE;
            }
            return cfg;
        }
    };

    class Pipeline {
    public:
        Pipeline(Device& device, ShaderSet& shaderSet, VkRenderPass renderPass, RenderTarget& target, const PipelineConfig& cfg);
        ~Pipeline();

        VkPipeline GetPipeline() const { return pipeline; };
        VkPipelineLayout GetLayout() const { return layout; };

        // Bind arbitrary data (UBO, texture, storage buffer) by descriptor name
        void BindData(const std::string& name, const void* data, size_t size, uint32_t arrayIndex = 0);
        void BindData(const std::string& name, const VkDescriptorImageInfo& imgInfo, uint32_t arrayIndex = 0);
        void BindMaterial(VkCommandBuffer cmd, Material& material, uint32_t frameIndex);


        // Binding per object Data
        void PushConstants(VkCommandBuffer cmd, const void* data, size_t size, VkShaderStageFlags stageFlags);


        // Update GPU buffers, call vkUpdateDescriptorSets internally
        void UpdateDescriptors(uint32_t frameIndex);
        const PushConstantRange* FindPushConstantRange(VkShaderStageFlags stageFlags);

        // Returns all descriptor sets for a given frame index
        inline const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const {
            return descriptorSets[frameIndex];
        }

        // For convenience if you only need the first set (like old code)
        inline VkDescriptorSet GetFirstDescriptorSet(uint32_t frameIndex) const {
            return descriptorSets[frameIndex].empty() ? VK_NULL_HANDLE : descriptorSets[frameIndex][0];
        }

        VkDescriptorPool getDiscriptorPool() { return descriptorPool; }

    private:
        ShaderSet* shaderSet;
        Device* device;
        RenderTarget& target;
        PipelineConfig config;

        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;

        std::vector<VkDescriptorSetLayout> setLayouts;
        std::vector<std::vector<VkDescriptorSet>> descriptorSets;

        std::unordered_map<std::string, BindingInfo> bindings;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;


        void CreatePipeline(VkRenderPass renderPass, const PipelineConfig& cfg);
        void CreateDescriptorSets();
        void CreateDescriptorPool();
        void PopulateBindingsFromShaderSet();
    };
}