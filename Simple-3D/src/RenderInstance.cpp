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

		// Draw Models
		for (Model* model : models) {
			if (!model->hasBuffer()) {
				model->CreateBuffers(RenderDevice, &commandPool);
			}

			// Bind pipeline for render pass
			auto pipeline = materials[model->material];
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());
			



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
			vkCmdSetViewport(cmd, 0, 1, &viewport);

			// Define sissor
			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			if (RenderToTexture) {
				scissor.extent = texture->getExtent();
			}
			else {
				scissor.extent = swapChain->GetSwapChainExtent();
			}


			vkCmdSetScissor(cmd, 0, 1, &scissor);

			// UPdate with models position and shizzle
			pipeline->updateUniformBuffer(currentFrame, camera->getProjectionMatrix(swapChain->GetSwapChainExtent().width, swapChain->GetSwapChainExtent().height), camera->getViewMatrix(), model->GetTransform(), camera->position);
			pipeline->updateLights(currentFrame, lights);							 

			VkBuffer vertexBuffers[] = { model->GetVertexBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(cmd, model->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			uint32_t dynamicOffset = 0;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(), 0, 1, &pipeline->descriptorSets[currentFrame], 1, &dynamicOffset);

			vkCmdDrawIndexed(cmd, static_cast<uint32_t>(model->Indices.size()), 1, 0, 0, 0);
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