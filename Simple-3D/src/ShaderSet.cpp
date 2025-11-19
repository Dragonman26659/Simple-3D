#include "Internal/Material/ShaderSet.h"




namespace Simple3D {
    // If you use SPIRV-Reflect's enum directly:
    inline VkShaderStageFlagBits spvReflectShaderStageToVk(SpvReflectShaderStageFlagBits s)
    {
        switch (s) {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:       return VK_SHADER_STAGE_VERTEX_BIT;
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:     return VK_SHADER_STAGE_FRAGMENT_BIT;
        case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:     return VK_SHADER_STAGE_GEOMETRY_BIT;
        case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:      return VK_SHADER_STAGE_COMPUTE_BIT;
        default: return static_cast<VkShaderStageFlagBits>(0);
        }
    }

    // If you want to map your own ShaderStage enum (from earlier message):
    inline VkShaderStageFlagBits spvReflectShaderStageToVk(ShaderStage s)
    {
        switch (s) {
        case ShaderStage::Vertex:      return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment:    return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Geometry:    return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::TessControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessEval:    return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Compute:     return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::RayGen:      return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case ShaderStage::AnyHit:      return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case ShaderStage::ClosestHit:  return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case ShaderStage::Miss:        return VK_SHADER_STAGE_MISS_BIT_KHR;
        case ShaderStage::Intersection:return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case ShaderStage::Callable:    return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        default: return static_cast<VkShaderStageFlagBits>(0);
        }
    }

    inline VkDescriptorType SpvToVkDescriptorType(SpvReflectDescriptorType type)
    {
        switch (type)
        {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        default:
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    const char* spvTypeName(SpvReflectDescriptorType t) {
        switch (t) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: return "SAMPLER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "COMBINED_IMAGE_SAMPLER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return "SAMPLED_IMAGE";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: return "STORAGE_IMAGE";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return "UNIFORM_TEXEL_BUFFER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return "STORAGE_TEXEL_BUFFER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return "UNIFORM_BUFFER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: return "STORAGE_BUFFER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return "UNIFORM_BUFFER_DYNAMIC";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return "STORAGE_BUFFER_DYNAMIC";
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return "INPUT_ATTACHMENT";
        default: return "UNKNOWN";
        }
    }

    // helper: convert ShaderStage (or SpvReflect stage) to string
    const char* StageName(ShaderStage s) {
        switch (s) {
        case ShaderStage::Vertex: return "VERTEX";
        case ShaderStage::Fragment: return "FRAGMENT";
        case ShaderStage::Geometry: return "GEOMETRY";
        case ShaderStage::TessControl: return "TESS_CTRL";
        case ShaderStage::TessEval: return "TESS_EVAL";
        case ShaderStage::Compute: return "COMPUTE";
        default: return "UNKNOWN";
        }
    }


    ShaderSet::ShaderSet(VkDevice device, std::string name) : device(device), name(name) {
    }


    // load a shader stage
    bool ShaderSet::LoadStage(const std::string& path, ShaderStage stage) {
        // Read SPIR-V file
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) return false;
        size_t fileSize = (size_t)file.tellg();
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        // Create shader module
        VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = buffer.size() * sizeof(uint32_t);
        createInfo.pCode = buffer.data();

        VkShaderModule module;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
            return false;

        stages.push_back({ stage, module, path, std::move(buffer) });
        return true;
    }

    bool ShaderSet::Reflect() {
        for (auto& stageInfo : stages) {
            SpvReflectShaderModule reflModule;
            spvReflectCreateShaderModule(stageInfo.spirv.size() * sizeof(uint32_t), stageInfo.spirv.data(), &reflModule);

            uint32_t setCount = 0;
            spvReflectEnumerateDescriptorSets(&reflModule, &setCount, nullptr);
            std::vector<SpvReflectDescriptorSet*> sets(setCount);
            spvReflectEnumerateDescriptorSets(&reflModule, &setCount, sets.data());

            VkShaderStageFlags stageFlag = static_cast<VkShaderStageFlags>(spvReflectShaderStageToVk(stageInfo.stage));

            for (auto* set : sets) {
                for (uint32_t i = 0; i < set->binding_count; ++i) {
                    const auto* b = set->bindings[i];
                    uint32_t setIndex = set->set;
                    uint32_t bindingIndex = b->binding;

                    descriptorSets[setIndex].push_back({
                        bindingIndex,
                        setIndex,
                        SpvToVkDescriptorType(b->descriptor_type),
                        b->count,
                        b->name ? b->name : "UNKNOWN_BINDING",
                        stageFlag
                        });
                }
            }

            // Push constants
            uint32_t pcCount = 0;
            spvReflectEnumeratePushConstantBlocks(&reflModule, &pcCount, nullptr);
            std::vector<SpvReflectBlockVariable*> pcs(pcCount);
            spvReflectEnumeratePushConstantBlocks(&reflModule, &pcCount, pcs.data());
            for (auto* pc : pcs) {
                pushConstants.push_back({
                    static_cast<VkShaderStageFlags>(spvReflectShaderStageToVk(stageInfo.stage)),
                    pc->offset,
                    pc->size
                    });
            }

            spvReflectDestroyShaderModule(&reflModule);
        }


        CreateDescriptorSetLayouts();


        return true;
    }


    VkPipelineLayout ShaderSet::CreatePipelineLayout() {
        if (setLayouts.empty())
            CreateDescriptorSetLayouts();

        VkPipelineLayoutCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        info.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        info.pSetLayouts = setLayouts.data();
        std::vector<VkPushConstantRange> vkPushConstants;
        vkPushConstants.reserve(pushConstants.size());

        for (const auto& pc : pushConstants) {
            VkPushConstantRange vkRange{};
            vkRange.stageFlags = pc.stageFlags;
            vkRange.offset = pc.offset;
            vkRange.size = pc.size;
            vkPushConstants.push_back(vkRange);
        }

        info.pushConstantRangeCount = static_cast<uint32_t>(vkPushConstants.size());
        info.pPushConstantRanges = vkPushConstants.empty() ? nullptr : vkPushConstants.data();

        vkCreatePipelineLayout(device, &info, nullptr, &pipelineLayout);
        return pipelineLayout;
    }





    const std::vector<VkDescriptorSetLayout>& ShaderSet::GetDescriptorSetLayouts() {
        if (setLayouts.empty()) {
            CreateDescriptorSetLayouts();
        }
        return setLayouts;
    }

    void ShaderSet::CreateDescriptorSetLayouts() {
        setLayouts.clear();

        for (auto& [set, bindings] : descriptorSets) {
            std::vector<VkDescriptorSetLayoutBinding> vkBindings;
            for (auto& b : bindings) {
                VkDescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding = b.binding;
                layoutBinding.descriptorType = b.type;
                layoutBinding.descriptorCount = b.count;
                layoutBinding.stageFlags = b.stageFlags;
                layoutBinding.pImmutableSamplers = nullptr;
                vkBindings.push_back(layoutBinding);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
            layoutInfo.pBindings = vkBindings.data();

            VkDescriptorSetLayout layout;
            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create descriptor set layout");
            }
            setLayouts.push_back(layout);
        }
    }

    VkGraphicsPipelineCreateInfo ShaderSet::BuildPipelineInfo(VkRenderPass renderPass, VkPipelineLayout pipelineLayout) {
        // Prepare shader stage create infos
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages(stages.size());
        for (size_t i = 0; i < stages.size(); ++i) {
            shaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[i].stage = spvReflectShaderStageToVk(stages[i].stage);
            shaderStages[i].module = stages[i].module;
            shaderStages[i].pName = "main";
        }

        // Vertex input description (assuming fixed vertex layout)
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        static VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        static VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Create basic pipeline info
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        return pipelineInfo;
    }


    std::vector<VkPipelineShaderStageCreateInfo> ShaderSet::GetShaderStageCreateInfos() const {
        std::vector<VkPipelineShaderStageCreateInfo> out(stages.size());
        for (size_t i = 0; i < stages.size(); ++i) {
            out[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            out[i].stage = spvReflectShaderStageToVk(stages[i].stage);
            out[i].module = stages[i].module;
            out[i].pName = "main";
        }
        return out;
    }

    std::vector<VkPipelineShaderStageCreateInfo> ShaderSet::GetShaderStages() const {
        std::vector<VkPipelineShaderStageCreateInfo> stagesInfo;
        stagesInfo.reserve(stages.size());
        for (const auto& s : stages) {
            VkPipelineShaderStageCreateInfo stageInfo{};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = spvReflectShaderStageToVk(s.stage);
            stageInfo.module = s.module;
            stageInfo.pName = "main";
            stagesInfo.push_back(stageInfo);
        }
        return stagesInfo;
    }
}