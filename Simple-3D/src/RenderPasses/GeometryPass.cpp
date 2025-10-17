#include "Internal/RenderPasses/GeometryPass.h"



namespace Simple3D {
    GeometryPass::GeometryPass(Camera* camera) {
        this->type = PassType::Render;
        this->name = "GeometryPass";
        this->camera = camera;
    }

    void GeometryPass::Execute(VkCommandBuffer cmd, const RenderData& data, uint32_t imageIndex) {
        if (!renderInfo.target.IsValid()) {
            throw std::runtime_error("Invalid render target in GeometryPass!");
        }

        RenderTarget& target = renderInfo.target;
        const VkExtent2D extent = target.GetExtent();

        // --- 1. Clear values (color + optional depth)
        std::vector<VkClearValue> clearValues;
        VkClearValue colorClear{};
        colorClear.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues.push_back(colorClear);

        if (target.HasDepth()) {
            // Confirm they are both the same size
            if (target.depthTexture->extent.width != extent.width ||
                target.depthTexture->extent.height != extent.height) {
                delete target.depthTexture;
                target.depthTexture = new DepthBuffer(device, extent, commandPool);
            }


            VkClearValue depthClear{};
            depthClear.depthStencil = { 1.0f, 0 };
            clearValues.push_back(depthClear);
        }

        // --- 2. Transition target texture for write (if offscreen)
        if (target.IsTexture()) {
            target.texture->TransitionForWrite(cmd, 0);
        }

        // --- 3. Begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
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

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = extent;

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // --- 5. Main geometry rendering
        // Group models by material to minimize state changes
        std::unordered_map<Material*, std::vector<Model*>> modelsByMaterial;
        for (Model* model : data.models) {
            modelsByMaterial[model->material].push_back(model);
        }

        // Render each group of models with the same material
        for (const auto& [material, modelGroup] : modelsByMaterial) {
            auto pipeline = renderInfo.materialPipelines[material];

            // Ensure light buffer / descriptors are updated BEFORE binding the set
            pipeline->updateLights(imageIndex, data.lights);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());


            // bind descriptors once per material, but still pass dynamic offset per-draw
            for (size_t idx = 0; idx < modelGroup.size(); ++idx) {
                Model* model = modelGroup[idx];

                if (!model->hasBuffer()) {
                    model->CreateBuffers(device, commandPool);
                }


                // update into dynamic UBO slot idx (or some object index assignment logic)
                pipeline->updateUniformBuffer(imageIndex, static_cast<uint32_t>(idx),
                    camera->getProjectionMatrix((int)viewport.width, (int)viewport.height),
                    camera->getViewMatrix(),
                    model->GetTransform(),
                    camera->position);

                // compute dynamic offset in bytes (must be multiple of dynamicAlignment)
                VkDeviceSize dynamicAlignment = getAlignedSize(sizeof(UniformBufferObject), device->GetProperties().limits.minUniformBufferOffsetAlignment);
                uint32_t uboOffset = static_cast<uint32_t>(idx * dynamicAlignment);

                if (material->isLit) {
                    uint32_t lightOffset = 0; // if you don't use per-light offsets, keep 0
                    uint32_t offsets[2] = { uboOffset, lightOffset };
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(),
                        0, 1, &pipeline->descriptorSets[imageIndex],
                        2, offsets);
                }
                else {
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(),
                        0, 1, &pipeline->descriptorSets[imageIndex],
                        1, &uboOffset);
                }

                // bind vertex/index buffers & draw
                VkBuffer vertexBuffers[] = { model->GetVertexBuffer() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(cmd, model->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, static_cast<uint32_t>(model->Indices.size()), 1, 0, 0, 0);
            }
        }

        // --- 6. End render pass
        vkCmdEndRenderPass(cmd);

        // --- 7. Transition for sampling in post-processing or further passes
        if (target.IsTexture()) {
            target.texture->TransitionForRead(cmd, 0);
        }
    }

}