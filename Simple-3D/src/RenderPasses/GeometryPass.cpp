#include "Internal/RenderPasses/GeometryPass.h"



namespace Simple3D {
    ForwardPass::ForwardPass(Camera* camera) {
        this->type = PassType::Render;
        this->name = "ForwardPass";
        this->camera = camera;
    }

    void ForwardPass::SetUpPass() {
        // --- Configure the pipeline for a forward (geometry) pass ---
        uint32_t colorAttachmentCount = renderInfo.target.GetColorAttachmentCount();
        PassConfig = PipelineConfig::MakeDefaultForAttachments(colorAttachmentCount);

        // Vertex input enabled (geometry)
        PassConfig.useVertexInput = true;
        PassConfig.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Rasterizer
        PassConfig.cullMode = VK_CULL_MODE_BACK_BIT;
        PassConfig.frontFace = VK_FRONT_FACE_CLOCKWISE;
        PassConfig.polygonMode = VK_POLYGON_MODE_FILL;
        PassConfig.depthClampEnable = VK_FALSE;
        PassConfig.rasterizerDiscardEnable = VK_FALSE;
        PassConfig.lineWidth = 1.0f;

        // Depth test/write (standard forward rendering)
        PassConfig.depthTestEnable = true;
        PassConfig.depthWriteEnable = true;
        PassConfig.depthCompareOp = VK_COMPARE_OP_LESS;

        // Multisampling (if you later add MSAA support)
        PassConfig.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        PassConfig.sampleShadingEnable = VK_FALSE;

        // Color blending: no transparency by default, but can be configured per material
        for (auto& att : PassConfig.blendAttachments) {
            att.blendEnable = VK_FALSE;
            att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        // Dynamic states
        PassConfig.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        // Subpass (0 for single-pass)
        PassConfig.depthTestEnable = renderInfo.target.HasDepth();
        PassConfig.depthWriteEnable = renderInfo.target.HasDepth();
        PassConfig.subpass = 0;
    }

    void ForwardPass::Execute(VkCommandBuffer cmd, const RenderData& data, uint32_t imageIndex, uint32_t syncIndex) {
        if (!renderInfo.target.IsValid()) {
            throw std::runtime_error("Invalid render target in GeometryPass!");
        }

        renderInfo.target.TransitionForWrite(cmd, imageIndex);

        RenderTarget& target = renderInfo.target;
        const VkExtent2D extent = target.GetExtent();

        // --- 1. Clear values
        std::vector<VkClearValue> clearValues;
        VkClearValue colorClear{};
        colorClear.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues.push_back(colorClear);

        if (target.HasDepth()) {
            // ensure depth buffer matches extent
            if (target.depthTexture->extent.width != extent.width ||
                target.depthTexture->extent.height != extent.height) {
                delete target.depthTexture;
                target.depthTexture = new DepthBuffer(device, extent, commandPool);
            }

            VkClearValue depthClear{};
            depthClear.depthStencil = { 1.0f, 0 };
            clearValues.push_back(depthClear);
        }

        // --- 2. Transition target texture for write
        if (target.IsTexture()) {
            target.TransitionForWrite(cmd, 0);
        }

        // --- 3. Begin render pass
        VkRenderPassBeginInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = renderInfo.GetFrameBuffer(imageIndex);
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = extent;
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // --- 4. Viewport + Scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{ {0, 0}, extent };

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // --- 5. Group models by shader set
        std::unordered_map<ShaderSet*, std::vector<Model*>> modelsByShaders;
        for (Model* model : data.models) {
            modelsByShaders[model->material->shaders].push_back(model);
        }

        // --- 6. Render each shader group
        for (const auto& [shaderSet, modelGroup] : modelsByShaders) {
            Pipeline* pipeline = renderInfo.Pipelines[shaderSet];
            if (!pipeline) continue;

            // Update per-frame (camera + lights)
            pipeline->BindData("lightBuffer", data.lights.data(), sizeof(Light) * data.lights.size());

            pipeline->UpdateDescriptors(syncIndex);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());

            // Bind global descriptor sets (all sets for this frame)
            const auto& sets = pipeline->GetDescriptorSets(imageIndex);
            if (!sets.empty()) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(),
                    0, static_cast<uint32_t>(sets.size()), sets.data(),
                    0, nullptr);
            }

            // Draw each model in this group
            for (Model* model : modelGroup) {
                if (!model->hasBuffer()) {
                    model->CreateBuffers(device, commandPool);
                }

                // --- Per-object UBO
                UniformBufferObject objectUBO{};
                objectUBO.model = model->GetTransform();
                objectUBO.view = camera->getViewMatrix();
                objectUBO.proj = camera->getProjectionMatrix(extent.width, extent.height);
                objectUBO.cameraPos = camera->getPosition();

                pipeline->PushConstants(cmd, &objectUBO, sizeof(objectUBO), VK_SHADER_STAGE_VERTEX_BIT);

                // --- Bind material textures
                pipeline->BindMaterial(cmd, *model->material, imageIndex);

                pipeline->UpdateDescriptors(syncIndex);

                // --- Bind vertex/index buffers and draw
                VkBuffer vertexBuffers[] = { model->GetVertexBuffer() };
                VkDeviceSize vbOffsets[] = { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, vbOffsets);
                vkCmdBindIndexBuffer(cmd, model->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(cmd, static_cast<uint32_t>(model->Indices.size()), 1, 0, 0, 0);
            }
        }

        // --- 7. End render pass
        vkCmdEndRenderPass(cmd);

        // --- 8. Transition for sampling
        if (target.IsTexture()) {
            target.TransitionForRead(cmd, imageIndex);
		}
    }
}