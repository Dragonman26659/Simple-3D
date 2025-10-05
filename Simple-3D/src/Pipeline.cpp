#include "Internal/Pipeline.h"




namespace Simple3D {
    Pipeline::Pipeline(Device& device, VkRenderPass renderPass, Material* materialBinding) : s_Device(device), renderPass(renderPass), material(materialBinding) {

        vertShaderCode = readFile(material->vertexSource);
        fragShaderCode = readFile(material->FragmentSource);


        createDescriptorSetLayout();
        CreatePipeline();

        createUniformBuffers();
        createLightBuffers();
        createDescriptorPool();
        createDescriptorSets();
    }

    Pipeline::~Pipeline() {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(s_Device.getLogicalDevice(), uniformBuffers[i], nullptr);
            vkFreeMemory(s_Device.getLogicalDevice(), uniformBuffersMemory[i], nullptr);
        }

        // Clean up light buffers
        if (material->isLit) {
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                if (lightBuffers[i] != VK_NULL_HANDLE) {
                    vkDestroyBuffer(s_Device.getLogicalDevice(), lightBuffers[i], nullptr);
                }
                if (lightBuffersMemory[i] != VK_NULL_HANDLE) {
                    vkFreeMemory(s_Device.getLogicalDevice(), lightBuffersMemory[i], nullptr);
                }
            }
        }

        vkDestroyDescriptorPool(s_Device.getLogicalDevice(), descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(s_Device.getLogicalDevice(), descriptorSetLayout, nullptr);

        vkDestroyPipeline(s_Device.getLogicalDevice(), graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(s_Device.getLogicalDevice(), pipelineLayout, nullptr);
    }

    std::vector<char> Pipeline::readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            printf("Failed to open shader file");
            throw std::runtime_error("failed to open file!");
        }

        std::streamsize fileSize = file.tellg();
        if (fileSize < 0) {
            throw std::runtime_error("Invalid file size");
        }

        std::vector<char> buffer(static_cast<size_t>(fileSize));
        file.seekg(0);

        file.read(buffer.data(), buffer.size());
        if (!file) {
            throw std::runtime_error("Failed to read file");
        }

        return buffer;
    }


    void Pipeline::CreatePipeline() {
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // Vertex input state with interleaved attributes
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        if (vkCreatePipelineLayout(s_Device.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pDepthStencilState = &depthStencil;

        if (vkCreateGraphicsPipelines(s_Device.getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(s_Device.getLogicalDevice(), fragShaderModule, nullptr);
        vkDestroyShaderModule(s_Device.getLogicalDevice(), vertShaderModule, nullptr);
    }

    VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{}; 
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size(); 
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); 

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(s_Device.getLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            printf("failed to create shader module!");
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void Pipeline::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;

        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = static_cast<uint32_t>(material->textures.size());
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding lightLayoutBinding{};
        lightLayoutBinding.binding = 2;
        lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        lightLayoutBinding.descriptorCount = 1;
        lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        lightLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
            uboLayoutBinding,
            samplerLayoutBinding,
            lightLayoutBinding
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(s_Device.getLogicalDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }


    void Pipeline::createDescriptorPool() {
        // Create pool sizes vector
        std::vector<VkDescriptorPoolSize> poolSizes;

        // Uniform buffer pool size
        poolSizes.push_back({});
        poolSizes.back().type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes.back().descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        // Combined image sampler pool size
        if (!material->textures.empty()) {
            poolSizes.push_back({});
            poolSizes.back().type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSizes.back().descriptorCount = static_cast<uint32_t>(material->textures.size()) * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        }

        if (material->isLit) {
            poolSizes.push_back({});
            poolSizes.back().type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            poolSizes.back().descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        }


        // Create descriptor pool
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(s_Device.getLogicalDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void Pipeline::createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(s_Device.getLogicalDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            if (!material->textures.empty()) {
                std::vector<VkWriteDescriptorSet> descriptorWrites;
                std::vector<VkDescriptorImageInfo> descriptorImageInfos;



                material->updateSortedTextureNames();
                descriptorImageInfos.reserve(material->textures.size());

                for (const auto& textureName : material->sortedTextureNames) {
                    const auto& binding = material->textures[textureName];
                    descriptorImageInfos.emplace_back(VkDescriptorImageInfo());
                    descriptorImageInfos.back().imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    descriptorImageInfos.back().imageView = binding.view;
                    descriptorImageInfos.back().sampler = binding.sampler;
                }

                // Single update for all textures
                VkWriteDescriptorSet textureWrite{};
                textureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                textureWrite.dstSet = descriptorSets[i];
                textureWrite.dstBinding = 1;
                textureWrite.dstArrayElement = 0;
                textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                textureWrite.descriptorCount = static_cast<uint32_t>(material->textures.size());
                textureWrite.pImageInfo = descriptorImageInfos.data();

                descriptorWrites.push_back(textureWrite);

                // Update descriptors before drawing
                vkUpdateDescriptorSets(s_Device.getLogicalDevice(),
                    static_cast<uint32_t>(descriptorWrites.size()),
                    descriptorWrites.data(),
                    0, nullptr);
            }

            // Create descriptor writes
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            // Update uniform buffer descriptor
            vkUpdateDescriptorSets(s_Device.getLogicalDevice(), 1, &descriptorWrite, 0, nullptr);



            std::vector<VkWriteDescriptorSet> descriptorWrites;
            VkDescriptorBufferInfo uboBufferInfo{};
            uboBufferInfo.buffer = uniformBuffers[i];
            uboBufferInfo.offset = 0;
            uboBufferInfo.range = sizeof(UniformBufferObject);

            if (material->isLit) {  
                VkDescriptorBufferInfo lightBufferInfo{};
                lightBufferInfo.buffer = lightBuffers[i];
                lightBufferInfo.offset = 0;
                lightBufferInfo.range = sizeof(Light);

                // Light buffer descriptor
                VkWriteDescriptorSet lightWrite{};
                lightWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                lightWrite.dstSet = descriptorSets[i];
                lightWrite.dstBinding = 2;
                lightWrite.dstArrayElement = 0;
                lightWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                lightWrite.descriptorCount = 1;
                lightWrite.pBufferInfo = &lightBufferInfo;

                descriptorWrites.push_back(lightWrite);
            }

            // Uniform buffer descriptor
            VkWriteDescriptorSet uboWrite{};
            uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uboWrite.dstSet = descriptorSets[i];
            uboWrite.dstBinding = 0;
            uboWrite.dstArrayElement = 0;
            uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboWrite.descriptorCount = 1;
            uboWrite.pBufferInfo = &uboBufferInfo;
                
            descriptorWrites.push_back(uboWrite);


            vkUpdateDescriptorSets(s_Device.getLogicalDevice(),
                static_cast<uint32_t>(descriptorWrites.size()),
                descriptorWrites.data(),
                0, nullptr);

            //vkUpdateDescriptorSets(s_Device.getLogicalDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }


    VkPipeline Pipeline::GetPipeline() {
        return graphicsPipeline;
    }

    void Pipeline::createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i], &s_Device);

            vkMapMemory(s_Device.getLogicalDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
        }
    }

    void Pipeline::updateUniformBuffer(uint32_t currentImage, glm::mat4 PerspectiveMatrix, glm::mat4 ViewMatrix, glm::mat4 transform, glm::vec3 CameraPos) {
        UniformBufferObject ubo{};
        ubo.view = ViewMatrix;
        ubo.proj = PerspectiveMatrix;
        ubo.model = transform;
        ubo.cameraPos = CameraPos;

        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void Pipeline::createLightBuffers() {
        if (!material->isLit)
            return;

        VkDeviceSize bufferSize = sizeof(Light);
        lightBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        lightBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        lightBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
        lightBuffersSize.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                lightBuffers[i], lightBuffersMemory[i], &s_Device);

            vkMapMemory(s_Device.getLogicalDevice(), lightBuffersMemory[i], 0, bufferSize, 0, &lightBuffersMapped[i]);
        }
    }

    void Pipeline::updateLights(uint32_t currentImage, const std::vector<Light>& lights) {
        if (!material->isLit)
            return;

        // Calculate required buffer size
        VkDeviceSize bufferSize = sizeof(Light) * lights.size();
        VkPhysicalDevice physicalDevice = s_Device.getPhysicalDevice();
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        // Align buffer size
        bufferSize = (bufferSize + properties.limits.minUniformBufferOffsetAlignment - 1) &
            ~(properties.limits.minUniformBufferOffsetAlignment - 1);

        // Create new buffer if size has changed
        if (lightBuffers[currentImage] == VK_NULL_HANDLE || bufferSize != lightBuffersSize[currentImage]) {
            // Clean up old buffer if it exists
            if (lightBuffers[currentImage] != VK_NULL_HANDLE) {
                vkDestroyBuffer(s_Device.getLogicalDevice(), lightBuffers[currentImage], nullptr);
            }

            if (lightBuffersMemory[currentImage] != VK_NULL_HANDLE) {
                vkFreeMemory(s_Device.getLogicalDevice(), lightBuffersMemory[currentImage], nullptr);
            }

            lightBuffersMapped[currentImage] = nullptr;

            // Create new buffer
            createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                lightBuffers[currentImage], lightBuffersMemory[currentImage], &s_Device);

            // Map the memory
            vkMapMemory(s_Device.getLogicalDevice(), lightBuffersMemory[currentImage], 0, bufferSize, 0, &lightBuffersMapped[currentImage]);

            lightBuffersSize[currentImage] = bufferSize;
        }

        // Copy lights to buffer
        memcpy(lightBuffersMapped[currentImage], lights.data(), lights.size() * sizeof(Light));

        // Update the descriptor set to reference the new buffer
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = lightBuffers[currentImage];
        bufferInfo.offset = 0;
        bufferInfo.range = bufferSize;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSets[currentImage];
        write.dstBinding = 2;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(s_Device.getLogicalDevice(), 1, &write, 0, nullptr);
    }

    VkPipelineLayout	Pipeline::GetLayout() {
        return pipelineLayout;
    }



    void Pipeline::setRenderPass(VkRenderPass newRenderPass) {
        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(s_Device.getLogicalDevice(), graphicsPipeline, nullptr);
        }

        renderPass = newRenderPass;

        CreatePipeline();
    }
}