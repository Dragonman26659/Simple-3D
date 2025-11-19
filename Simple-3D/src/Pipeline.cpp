#include "Internal/Pipeline.h"




namespace Simple3D {
    Pipeline::Pipeline(Device& device, ShaderSet& shaderSet, VkRenderPass renderPass, RenderTarget& target, const PipelineConfig& cfg)
        : device(&device), shaderSet(&shaderSet), target(target), config(cfg) {
        // create layout from shaderSet reflection (unchanged)
        layout = shaderSet.CreatePipelineLayout();

        // create pipeline object using config
        CreatePipeline(renderPass, config);

        PopulateBindingsFromShaderSet();
        CreateDescriptorSets();
    }


    

    Pipeline::~Pipeline() {
        for (auto& [name, b] : bindings) {
            // unmap memory for all frames
            for (size_t i = 0; i < b.bufferMemories.size(); ++i) {
                if (b.mappedData.size() > i && b.mappedData[i]) {
                    vkUnmapMemory(device->getLogicalDevice(), b.bufferMemories[i]);
                    b.mappedData[i] = nullptr;
                }
                if (b.buffers.size() > i && b.buffers[i] != VK_NULL_HANDLE) {
                    vkDestroyBuffer(device->getLogicalDevice(), b.buffers[i], nullptr);
                }
                if (b.bufferMemories.size() > i && b.bufferMemories[i] != VK_NULL_HANDLE) {
                    vkFreeMemory(device->getLogicalDevice(), b.bufferMemories[i], nullptr);
                }
            }
        }

        for (auto layout : setLayouts) {
            vkDestroyDescriptorSetLayout(device->getLogicalDevice(), layout, nullptr);
        }

        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device->getLogicalDevice(), pipeline, nullptr);

        if (layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device->getLogicalDevice(), layout, nullptr);

        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device->getLogicalDevice(), descriptorPool, nullptr);
        }
    }

    void Pipeline::BindData(const std::string& name, const void* data, size_t size, uint32_t arrayIndex) {
        auto it = bindings.find(name);
        if (it == bindings.end()) {
            throw std::runtime_error("Binding name not found: " + name);
        }

        BindingInfo& binding = it->second;

        if (binding.type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
            binding.type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
            binding.type != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
            binding.type != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
            throw std::runtime_error("BindData called for non-buffer binding: " + name);
        }

        // allocate per-frame if not allocated yet
        if (binding.buffers.empty()) {
            binding.buffers.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
            binding.bufferMemories.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
            binding.mappedData.resize(MAX_FRAMES_IN_FLIGHT, nullptr);

            VkDeviceSize bufferSize = size;

            // optional: align bufferSize to minUniformBufferOffsetAlignment if you will use offsets
            VkPhysicalDeviceProperties props = device->GetProperties();
            VkDeviceSize minUboAlign = props.limits.minUniformBufferOffsetAlignment;
            if (binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
                if (minUboAlign > 0) {
                    bufferSize = (bufferSize + minUboAlign - 1) & ~(minUboAlign - 1);
                }
            }

            VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

            for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
                createBuffer(bufferSize, usage,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    binding.buffers[f], binding.bufferMemories[f], device);

                // map memory and keep mapped pointer
                void* mapped = nullptr;
                vkMapMemory(device->getLogicalDevice(), binding.bufferMemories[f], 0, bufferSize, 0, &mapped);
                binding.mappedData[f] = mapped;
            }

            binding.dataSize = size; // logical size, not necessarily the allocation size
        }

        // by default write to all frames (you can choose to only write the current frame)
        for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
            std::memcpy(binding.mappedData[f], data, size);
        }
    }

    void Pipeline::BindData(const std::string& name, const VkDescriptorImageInfo& imgInfo, uint32_t arrayIndex) {
        auto it = bindings.find(name);
        if (it == bindings.end())
            throw std::runtime_error("Binding not found: " + name);

        BindingInfo& bindInfo = it->second;

        if (bindInfo.type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER &&
            bindInfo.type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
            throw std::runtime_error("BindData called on non-image binding: " + name);
        }

        bindInfo.imageView = imgInfo.imageView;
        bindInfo.sampler = imgInfo.sampler; // sampler may be VK_NULL_HANDLE for SAMPLED_IMAGE
    }


    void Pipeline::UpdateDescriptors(uint32_t frameIndex) {
        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorBufferInfo> bufferInfos;  // temp storage so pointers remain valid
        std::vector<VkDescriptorImageInfo> imageInfos;

        bufferInfos.reserve(bindings.size());
        imageInfos.reserve(bindings.size());

        for (auto& [name, binding] : bindings) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSets[frameIndex][binding.set];
            write.dstBinding = binding.binding;
            write.dstArrayElement = 0;
            write.descriptorType = binding.type;
            write.descriptorCount = 1;

            if (binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {

                VkDescriptorBufferInfo bufInfo{};
                bufInfo.buffer = binding.buffers[frameIndex];
                bufInfo.offset = 0;
                bufInfo.range = binding.dataSize == 0 ? VK_WHOLE_SIZE : binding.dataSize;
                bufferInfos.push_back(bufInfo);
                write.pBufferInfo = &bufferInfos.back();
            }
            else if (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                binding.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {

                VkDescriptorImageInfo imgInfo{};
                imgInfo.imageView = binding.imageView;
                imgInfo.sampler = binding.sampler;
                imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfos.push_back(imgInfo);
                write.pImageInfo = &imageInfos.back();
            }

            writes.push_back(write);
        }

        vkUpdateDescriptorSets(device->getLogicalDevice(),
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0, nullptr);
    }

    void Pipeline::CreateDescriptorSets() {
        setLayouts.clear();
        descriptorSets.clear();

        const auto& reflLayouts = shaderSet->GetDescriptorSetLayouts();
        for (auto layout : reflLayouts) {
            setLayouts.push_back(layout);
        }

        // prepare descriptorSets as [frame][setCount]
        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            descriptorSets[i].resize(setLayouts.size());
        }

        CreateDescriptorPool();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorSetAllocateInfo alloc{};
            alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc.descriptorPool = descriptorPool;
            alloc.descriptorSetCount = static_cast<uint32_t>(setLayouts.size());
            alloc.pSetLayouts = setLayouts.data();

            // allocate into the address of the first element of the frame's vector
            if (vkAllocateDescriptorSets(device->getLogicalDevice(), &alloc, descriptorSets[i].data()) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate descriptor sets!");
            }
        }
    }

    void Pipeline::CreatePipeline(VkRenderPass renderPass, const PipelineConfig& cfg) {
        auto shaderStages = shaderSet->GetShaderStages();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = cfg.topology;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        if (cfg.useVertexInput) {
            auto bindingDescription = Vertex::getBindingDescription();
            auto attributeDescriptions = Vertex::getAttributeDescriptions();
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        }

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer.depthClampEnable = cfg.depthClampEnable;
        rasterizer.rasterizerDiscardEnable = cfg.rasterizerDiscardEnable;
        rasterizer.polygonMode = cfg.polygonMode;
        rasterizer.lineWidth = cfg.lineWidth;
        rasterizer.cullMode = cfg.cullMode;
        rasterizer.frontFace = cfg.frontFace;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.flags = cfg.rasterizerFlags;

        // Depth stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        bool hasDepth = target.HasDepth();
        if (hasDepth) {
            depthStencil.depthTestEnable = cfg.depthTestEnable ? VK_TRUE : VK_FALSE;
            depthStencil.depthWriteEnable = cfg.depthWriteEnable ? VK_TRUE : VK_FALSE;
            depthStencil.depthCompareOp = cfg.depthCompareOp;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;
        }

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampling.sampleShadingEnable = cfg.sampleShadingEnable;
        multisampling.rasterizationSamples = cfg.rasterizationSamples;

        // Viewport state (required even if dynamic)
        VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.pViewports = nullptr;
        viewportState.scissorCount = 1;
        viewportState.pScissors = nullptr;

        // Blending
        uint32_t attachmentCount = std::max<uint32_t>(1, target.GetColorAttachmentCount());
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments = cfg.blendAttachments.empty()
            ? PipelineConfig::MakeDefaultForAttachments(attachmentCount).blendAttachments
            : cfg.blendAttachments;

        VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
        colorBlending.pAttachments = blendAttachments.data();

        // Dynamic state
        VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount = static_cast<uint32_t>(cfg.dynamicStates.size());
        dynamicState.pDynamicStates = cfg.dynamicStates.data();

        // Build pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.pDepthStencilState = hasDepth ? &depthStencil : nullptr;
        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = cfg.subpass;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
    }


    inline void Pipeline::CreateDescriptorPool() {
        // accumulate counts per descriptor type
        std::unordered_map<VkDescriptorType, uint32_t> counts;
        for (auto& [name, binding] : bindings) {
            counts[binding.type] += binding.count * MAX_FRAMES_IN_FLIGHT;
        }

        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(counts.size());
        for (auto& [type, cnt] : counts) {
            VkDescriptorPoolSize ps{};
            ps.type = type;
            ps.descriptorCount = cnt;
            poolSizes.push_back(ps);
        }

        VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        // total number of descriptor sets (per-frame * number of set layouts)
        poolInfo.maxSets = static_cast<uint32_t>(setLayouts.size() * MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device->getLogicalDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool for pipeline");
        }
    }


    void Pipeline::PopulateBindingsFromShaderSet() {
        // Assuming ShaderSet exposes descriptorSets in a usable structure.
        // shaderSet->descriptorSets : std::map<uint32_t, std::vector<...>> or similar
        // Adapt to your ShaderSet::descriptorSets shape. The example below expects:
        // shaderSet->descriptorSets: std::map<uint32_t, std::vector<DescriptorEntry>>
        // where DescriptorEntry has fields: binding, set, type, count, name, stageFlags

        bindings.clear();

        // Access the map on ShaderSet (make it friend or accessor if needed).
        for (const auto& [setIndex, entries] : shaderSet->GetDescriptorMap()) {
            for (const auto& e : entries) {
                BindingInfo bi;
                bi.name = e.name;
                bi.binding = e.binding;
                bi.set = setIndex;
                bi.type = e.type;
                bi.count = e.count;
                bi.stageFlags = e.stageFlags;
                bindings[bi.name] = bi;
            }
        }
    }



    const PushConstantRange* Pipeline::FindPushConstantRange(VkShaderStageFlags stageFlags) {
        for (auto& pc : shaderSet->pushConstants) {
            if (pc.stageFlags & stageFlags) {
                return &pc;
            }
        }
        return nullptr;
    }




    void Pipeline::PushConstants(
        VkCommandBuffer cmd,
        const void* data,
        size_t size,
        VkShaderStageFlags stageFlags)
    {
        auto* range = FindPushConstantRange(stageFlags);
        if (!range) {
            printf("No push constant range for stage!");
            throw std::runtime_error("No push constant range for stage!");
        }

        vkCmdPushConstants(
            cmd,
            layout,
            stageFlags,
            range->offset,        // use SPIRV-reflect offset
            (uint32_t)size,
            data
        );
    }
}

