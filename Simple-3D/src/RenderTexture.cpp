#include "Internal/RenderTexture.h"


namespace Simple3D {
	RenderTexture::RenderTexture(Device* RenderDevice, SwapChain* swapChain)
		: RenderDevice(RenderDevice), swapChain(swapChain) {

		createRenderPassForImageView();
		createImageForImageView();
		createImageView();
		createFramebufferForImageView(swapChain->GetSwapChainExtent().width, swapChain->GetSwapChainExtent().height);
		width = swapChain->GetSwapChainExtent().width;
		height = swapChain->GetSwapChainExtent().height;
		setupTransitionResources();
	}

	RenderTexture::RenderTexture() {
	}

	RenderTexture::~RenderTexture() {
		vkDestroyFramebuffer(RenderDevice->getLogicalDevice(), framebuffer, nullptr);
		vkDestroyImageView(RenderDevice->getLogicalDevice(), imageView, nullptr);
		vkFreeMemory(RenderDevice->getLogicalDevice(), imageMemory, nullptr);
		vkDestroyImage(RenderDevice->getLogicalDevice(), image, nullptr);
	}



	void RenderTexture::createImageView() {

		// Setup image view creation info
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChain->GetSwapChainImageFormat();

		// Set up component mapping (swizzling)
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		// Set up subresource range
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		// Create the image view
		if (vkCreateImageView(RenderDevice->getLogicalDevice(), &createInfo, nullptr, &imageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}
	}

	void RenderTexture::createImageForImageView() {
		// Create image
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = swapChain->GetSwapChainImageFormat();
		imageInfo.extent = { swapChain->GetSwapChainExtent().width,
							swapChain->GetSwapChainExtent().height, 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT;

		if (vkCreateImage(RenderDevice->getLogicalDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		// Allocate memory
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(RenderDevice->getLogicalDevice(), image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, RenderDevice);

		if (vkAllocateMemory(RenderDevice->getLogicalDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		// Bind memory to image
		vkBindImageMemory(RenderDevice->getLogicalDevice(), image, imageMemory, 0);
	}

	void RenderTexture::createFramebufferForImageView(int width_n, int height_n) {
		// Create image view first
		width = width_n;
		height = height_n;


		if (!imageView) {
			throw std::runtime_error("failed to create image view!");
		}

		// Create framebuffer
		VkImageView attachments[] = { imageView };
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(RenderDevice->getLogicalDevice(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}




	void RenderTexture::createRenderPassForImageView() {
		VkAttachmentDescription attachment{};
		attachment.format = swapChain->GetSwapChainImageFormat();
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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


		if (vkCreateRenderPass(RenderDevice->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	TextureBinding* RenderTexture::getBinding() {
		binding = new TextureBinding();

		binding->view = imageView;
		binding->textureImage = image;
		binding->textureImageMemory = imageMemory;

		binding->width = width;
		binding->height = height;

		createTextureSampler(binding, RenderDevice);

		return binding;
	}


	void RenderTexture::cleanup() {
		vkDestroyFramebuffer(RenderDevice->getLogicalDevice(), framebuffer, nullptr);
		vkDestroyImageView(RenderDevice->getLogicalDevice(), imageView, nullptr);
		vkFreeMemory(RenderDevice->getLogicalDevice(), imageMemory, nullptr);
		vkDestroyImage(RenderDevice->getLogicalDevice(), image, nullptr);

		framebuffer = VK_NULL_HANDLE;
		imageView = VK_NULL_HANDLE;
		image = VK_NULL_HANDLE;
		imageMemory = VK_NULL_HANDLE;
	}

	bool RenderTexture::resize(int width, int height) {
		cleanup();

		createRenderPassForImageView();
		createImageForImageView();
		createImageView();
		createFramebufferForImageView(width, height);


		if (binding != nullptr) {
			binding->view = imageView;
			binding->textureImage = image;
			binding->textureImageMemory = imageMemory;
		}


		createTextureSampler(binding, RenderDevice);


		return true;
	}

	bool RenderTexture::transitionToPresentSrcKHR(VkCommandPool* cmdPool) {
		VkCommandBuffer cmdBuf = beginSingleTimeCommands(RenderDevice, cmdPool);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;     // Previous shader read access
		barrier.dstAccessMask = 0;                             // No access needed during present
		barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		// Pipeline stage transition
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		vkCmdPipelineBarrier(
			cmdBuf,
			srcStageMask, dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		endSingleTimeCommands(RenderDevice, cmdPool, &cmdBuf);
		vkResetFences(RenderDevice->getLogicalDevice(), 1, &transitionFence);

		return true;
	}

	bool RenderTexture::transitionToShaderReadOptimal(VkCommandPool* cmdPool) {
		VkCommandBuffer cmdBuf = beginSingleTimeCommands(RenderDevice, cmdPool);

		// Begin single-time command buffer
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdBuf, &beginInfo);

		// Image barrier
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = 0; // No source access needed
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		// Pipeline stage transition
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

		vkCmdPipelineBarrier(
			cmdBuf,
			srcStageMask, dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		endSingleTimeCommands(RenderDevice, cmdPool, &cmdBuf);
		vkResetFences(RenderDevice->getLogicalDevice(), 1, &transitionFence);

		return true;
	}
}