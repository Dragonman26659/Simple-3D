#pragma once
#include "SimpleCore.h"
#include "spirv_reflect.h"



namespace Simple3D {

    enum class ShaderStage {
        Vertex,
        Fragment,
        Geometry,
        TessControl,
        TessEval,
        Compute,
        RayGen,
        AnyHit,
        ClosestHit,
        Miss,
        Intersection,
        Callable
    };

    struct ShaderStageInfo {
        ShaderStage stage;
        VkShaderModule module;
        std::string path;
        std::vector<uint32_t> spirv; // raw SPIR-V for reflection
    };

    struct DescriptorBinding {
        uint32_t binding;
        uint32_t set;
        VkDescriptorType type;
        uint32_t count;
        std::string name;
        VkShaderStageFlags stageFlags = 0;
    };

    struct PushConstantRange {
        VkShaderStageFlags stageFlags;
        uint32_t offset;
        uint32_t size;
    };

    class ShaderSet {
    public:
        ShaderSet(VkDevice device, std::string name);


        bool LoadStage(const std::string& path, ShaderStage stage);
        bool Reflect();

        const std::vector<ShaderStageInfo>& GetStages() const { return stages; }
        const std::unordered_map<uint32_t, std::vector<DescriptorBinding>>& GetDescriptorSets() const { return descriptorSets; }

        // Information to create pipeline
        VkPipelineLayout CreatePipelineLayout();
        const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts();
        VkGraphicsPipelineCreateInfo BuildPipelineInfo(VkRenderPass renderPass, VkPipelineLayout pipelineLayout);
        std::vector<VkPipelineShaderStageCreateInfo> GetShaderStageCreateInfos() const;
        std::vector<VkPipelineShaderStageCreateInfo> GetShaderStages() const;

        std::unordered_map<uint32_t, std::vector<DescriptorBinding>> GetDescriptorMap() const { return descriptorSets; }

        std::vector<PushConstantRange> pushConstants;

    private:
        VkDevice device;
        std::vector<ShaderStageInfo> stages;
        std::unordered_map<uint32_t, std::vector<DescriptorBinding>> descriptorSets;
        std::string name;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> setLayouts;

        void CreateDescriptorSetLayouts();
    };
}