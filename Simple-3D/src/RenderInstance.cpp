#include "Internal/RenderInstance.h"


namespace Simple3D {
	RenderInstance::RenderInstance(Device* RenderDevice, SwapChain* swapChain, VkRenderPass renderPass, bool RenderToImgui)
		: RenderDevice(RenderDevice), swapChain(swapChain), renderPass(renderPass), RenderToImgui(RenderToImgui) {

		if (RenderToImgui) {
			renderPass = createRenderPassForImageView();
			createFramebufferForImageView();
		}
	}




	void RenderInstance::recordCommandBuffer(
		VkCommandBuffer cmd, uint32_t imageIndex, VkCommandPool commandPool
		, std::unordered_map<Material*, Pipeline*> materials, uint32_t currentFrame
		, VkFramebuffer mainFrameBuffer
		) {

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };


		// Get render pass and its information
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;

		// If rendering to an imgui window or not
		if (RenderToImgui)
			renderPassInfo.framebuffer = framebuffer;
		else
			renderPassInfo.framebuffer = mainFrameBuffer;

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChain->GetSwapChainExtent();
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
			viewport.width = static_cast<float>(swapChain->GetSwapChainExtent().width);
			viewport.height = static_cast<float>(swapChain->GetSwapChainExtent().height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(cmd, 0, 1, &viewport);

			// Define sissor
			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = swapChain->GetSwapChainExtent();
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			// UPdate with models position and shizzle
			pipeline->updateUniformBuffer(currentFrame, camera->getProjectionMatrix(swapChain->GetSwapChainExtent().width, swapChain->GetSwapChainExtent().height), camera->getViewMatrix(), model->GetTransform());
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
	}



	VkRenderPass RenderInstance::createRenderPassForImageView() {
		VkAttachmentDescription attachment{};
		attachment.format = swapChain->GetSwapChainImageFormat();
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &attachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VkRenderPass renderPass;
		if (vkCreateRenderPass(RenderDevice->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
		return renderPass;
	}

	void RenderInstance::createFramebufferForImageView() {
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &imageView;
		framebufferInfo.width = swapChain->GetSwapChainExtent().width;
		framebufferInfo.height = swapChain->GetSwapChainExtent().height;
		framebufferInfo.layers = 1;

		VkFramebuffer framebuffer;
		if (vkCreateFramebuffer(RenderDevice->getLogicalDevice(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}


	void RenderInstance::ClearVectors() {
		models.clear();
		lights.clear();
	}
}