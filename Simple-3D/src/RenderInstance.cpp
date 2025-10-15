#include "Internal/RenderInstance.h"


namespace Simple3D {
	RenderInstance::RenderInstance(Device* RenderDevice, SwapChain* swapChain, VkRenderPass renderPass, RenderTexture* texture)
		: RenderDevice(RenderDevice), swapChain(swapChain), renderPass(renderPass), RenderToTexture(true), texture(texture) {
	}


	RenderInstance::~RenderInstance() {
		// Cleanup materials (before render pass since they depend on it)
		for (const auto& pair : materials) {
			delete(pair.second);
			delete(pair.first);
		}
	}




	void RenderInstance::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, VkCommandPool commandPool, uint32_t currentFrame, VkFramebuffer mainFrameBuffer) {

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };


		// Get render pass and its information
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

		// If rendering to an imgui window or not
		if (RenderToTexture) {
			// Set texture so we render to it
			texture->TransitionForWrite(cmd, 0);

			renderPassInfo.framebuffer = texture->getFrameBuffer();
			renderPassInfo.renderPass = texture->getRenderPass();

			// Use texture dimensions for render area
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = texture->getExtent();;
		}
		else {
			renderPassInfo.framebuffer = mainFrameBuffer;
			renderPassInfo.renderPass = renderPass;

			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChain->GetSwapChainExtent();
		}
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();


		// Begin render pass
		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Define viewport
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		if (RenderToTexture) {
			viewport.width = static_cast<float>(texture->width);
			viewport.height = static_cast<float>(texture->height);
		}
		else {
			viewport.width = static_cast<float>(swapChain->GetSwapChainExtent().width);
			viewport.height = static_cast<float>(swapChain->GetSwapChainExtent().height);
		}
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;


		// Define sissor
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		if (RenderToTexture) {
			scissor.extent = texture->getExtent();
		}
		else {
			scissor.extent = swapChain->GetSwapChainExtent();
		}
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);


		// Group models by material to minimize state changes
		std::unordered_map<Material*, std::vector<Model*>> modelsByMaterial;
		for (Model* model : models) {
			modelsByMaterial[model->material].push_back(model);
		}

		// Render each group of models with the same material
		for (const auto& [material, modelGroup] : modelsByMaterial) {
			auto pipeline = materials[material];

			// Ensure light buffer / descriptors are updated BEFORE binding the set
			pipeline->updateLights(currentFrame, lights);

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());
		

			// bind descriptors once per material, but still pass dynamic offset per-draw
			for (size_t idx = 0; idx < modelGroup.size(); ++idx) {
				Model* model = modelGroup[idx];

				if (!model->hasBuffer()) {
					model->CreateBuffers(RenderDevice, &commandPool);
				}


				// update into dynamic UBO slot idx (or some object index assignment logic)
				pipeline->updateUniformBuffer(currentFrame, static_cast<uint32_t>(idx),
					camera->getProjectionMatrix((int)viewport.width, (int)viewport.height),
					camera->getViewMatrix(),
					model->GetTransform(),
					camera->position);

				// compute dynamic offset in bytes (must be multiple of dynamicAlignment)
				VkDeviceSize dynamicAlignment = getAlignedSize(sizeof(UniformBufferObject), RenderDevice->GetProperties().limits.minUniformBufferOffsetAlignment);
				uint32_t uboOffset = static_cast<uint32_t>(idx * dynamicAlignment);

				if (material->isLit) {
					uint32_t lightOffset = 0; // if you don't use per-light offsets, keep 0
					uint32_t offsets[2] = { uboOffset, lightOffset };
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(),
						0, 1, &pipeline->descriptorSets[currentFrame],
						2, offsets);
				}
				else {
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(),
						0, 1, &pipeline->descriptorSets[currentFrame],
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

		// End render pass
		vkCmdEndRenderPass(cmd);

		// If rendering to an imgui window or not
		if (RenderToTexture) {
			// Set texture so we can read from it
			texture->TransitionForRead(cmd, 0);
		}
	}


	void RenderInstance::ClearVectors() {
		models.clear();
		lights.clear();
	}

	// Creates a material given a material create struct, Material memory is handled by renderer
	void RenderInstance::CreateMaterial(Material* material, VkCommandPool commandPool) {
		// Add material to the materials map with a new pipeline

		if (RenderToTexture)
			materials[material] = new Pipeline(*RenderDevice, texture->getRenderPass(), material);
		else
			materials[material] = new Pipeline(*RenderDevice, renderPass, material);
	}
}