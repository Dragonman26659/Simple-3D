#include "Internal/RenderPasses/GeometryPass.h"



namespace Simple3D {
    GeometryPass::GeometryPass(Device* device, Camera* camera) {
        this->type = PassType::Render;
        this->name = "GeometryPass";
        this->device = device;
        this->camera = camera;
    }

    void GeometryPass::Execute(VkCommandBuffer cmd, RenderData data, uint32_t imageIndex) {
        if (!renderInfo.target.IsValid()) {
            throw std::runtime_error("Invalid render target in GeometryPass!");
        }

        const bool renderToTexture = renderInfo.target.texture != nullptr;
        RenderTexture* targetTexture = renderInfo.target.texture;
        SwapChain* swapchain = renderInfo.target.swapchain;

        // --- 1. Clear values (color + depth)
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        // --- 2. Render pass begin info
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        VkExtent2D extent{};

        if (renderToTexture) {
            targetTexture->TransitionForWrite(cmd, 0);
            renderPassInfo.framebuffer = renderInfo.framebuffer;
            renderPassInfo.renderPass = renderInfo.renderPass;
            extent = targetTexture->getExtent();
        }
        else {
            renderPassInfo.framebuffer = renderInfo.framebuffers[imageIndex];
            renderPassInfo.renderPass = renderInfo.renderPass;
            extent = swapchain->GetSwapChainExtent();
        }

        renderPassInfo.renderArea.extent = extent;

        // --- 3. Begin render pass
        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // --- 4. Set viewport & scissor
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

        // --- 5. Main Geometry Rendering ---
        for (auto& [material, pipeline] : renderInfo.materialPipelines) {
            pipeline->updateLights(imageIndex, data.lights);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());

            for (Model* model : data.models) {
                if (model->material != material)
                    continue;

                if (!model->hasBuffer())
                    model->CreateBuffers(device, nullptr);

                pipeline->updateUniformBuffer(
                    imageIndex,
                    0,
                    camera->getProjectionMatrix((int)extent.width, (int)extent.height),
                    camera->getViewMatrix(),
                    model->GetTransform(),
                    camera->position
                );

                uint32_t dynamicOffset = 0;
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->GetLayout(),
                    0, 1, &pipeline->descriptorSets[imageIndex],
                    1, &dynamicOffset);

                VkBuffer vb = model->GetVertexBuffer();
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, &vb, offsets);
                vkCmdBindIndexBuffer(cmd, model->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(cmd, static_cast<uint32_t>(model->Indices.size()), 1, 0, 0, 0);
            }
        }

        // --- 6. End render pass
        vkCmdEndRenderPass(cmd);

        // --- 7. Transition for post-processing / sampling
        if (renderToTexture)
            targetTexture->TransitionForRead(cmd, 0);
    }

}