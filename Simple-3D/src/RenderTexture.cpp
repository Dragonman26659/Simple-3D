#include "Internal/RenderTexture.h"


namespace Simple3D {
	RenderTexture::RenderTexture(Device* RenderDevice, int width, int height, VkFormat format)
		: RenderDevice(RenderDevice), width(width), height(height), format(format) {

		if (width <= 0 || height <= 0) {
			throw std::runtime_error("Invalid texture dimensions");
		}

		currentState.layout = VK_IMAGE_LAYOUT_UNDEFINED;
		currentState.owningQueueFamily = VK_QUEUE_FAMILY_IGNORED;
		currentState.lastTransitionFence = VK_NULL_HANDLE;

		// 1. Create color image
		createImageForImageView();
		createImageView();

		// 2. Create depth image
		createDepthResources();

		// 3. Create render pass (now includes depth attachment)
		createRenderPassForImageView();

		// 4. Create framebuffer (color + depth)
		createFramebufferForImageView(width, height);

		// 5. Setup transition / sync objects
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
		// First bind memory to the image if it hasn't been bound yet
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(RenderDevice->getLogicalDevice(), image, &memRequirements);
		if (imageMemory != VK_NULL_HANDLE &&
			memRequirements.size > 0) {
			vkBindImageMemory(RenderDevice->getLogicalDevice(), image, imageMemory, 0);
		}

		// Setup image view creation info
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = format;

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
		imageInfo.format = format;
		imageInfo.extent = { (unsigned int)width, (unsigned int)height, 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		
		imageInfo.usage =	VK_IMAGE_USAGE_SAMPLED_BIT |
							VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
							VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
							VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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

		currentState.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	void RenderTexture::createFramebufferForImageView(int width_n, int height_n) {
		// Create image view first
		width = width_n;
		height = height_n;


		if (!imageView) {
			throw std::runtime_error("failed to create image view!");
		}

		// Create framebuffer
		std::array<VkImageView, 2> attachments = { imageView, depthImageView };

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(RenderDevice->getLogicalDevice(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS)
			throw std::runtime_error("failed to create framebuffer!");
	}




	void RenderTexture::createRenderPassForImageView() {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pDepthStencilAttachment = &depthRef;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		if (vkCreateRenderPass(RenderDevice->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
			throw std::runtime_error("failed to create render pass for RenderTexture!");
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
		VkDevice device = RenderDevice->getLogicalDevice();

		if (depthImageView) vkDestroyImageView(device, depthImageView, nullptr);
		if (depthImage) vkDestroyImage(device, depthImage, nullptr);
		if (depthImageMemory) vkFreeMemory(device, depthImageMemory, nullptr);

		if (framebuffer) vkDestroyFramebuffer(device, framebuffer, nullptr);
		if (imageView) vkDestroyImageView(device, imageView, nullptr);
		if (image) vkDestroyImage(device, image, nullptr);
		if (imageMemory) vkFreeMemory(device, imageMemory, nullptr);

		if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
	}

	bool RenderTexture::resize(int nwidth, int nheight) {
		width, height = nwidth, nheight;
		cleanup();

		if (width <= 0 || height <= 0) {
			throw std::runtime_error("Invalid texture dimensions");
		}

		// 1. Initialize state first
		currentState.layout = VK_IMAGE_LAYOUT_UNDEFINED;
		currentState.owningQueueFamily = VK_QUEUE_FAMILY_IGNORED;
		currentState.lastTransitionFence = VK_NULL_HANDLE;


		// 2. Create image and view
		createImageForImageView();
		createImageView();

		// 3. Create render pass
		createRenderPassForImageView();

		// 4. Create framebuffer
		createFramebufferForImageView(width, height);

		// 5. Initialize transition resources
		setupTransitionResources();


		return true;
	}

	bool RenderTexture::TransitionForWrite(VkCommandBuffer cmdBuf, uint32_t targetQueueFamily) {
		std::lock_guard<std::mutex> lock(stateMutex);

		if (isTransitioning.exchange(true)) {
			throw std::runtime_error("Previous transition still in progress");
		}

		waitPreviousTransition();

		// Validate current state
		if (currentState.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
			// Transition from UNDEFINED to TRANSFER_DST_OPTIMAL
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = 0; 
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			// Ensure proper pipeline synchronization
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

			vkCmdPipelineBarrier(
				cmdBuf,
				srcStageMask,
				dstStageMask,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			currentState.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		}

		if (currentState.owningQueueFamily != VK_QUEUE_FAMILY_IGNORED &&
			currentState.owningQueueFamily != targetQueueFamily) {
			throw std::runtime_error("Invalid queue family transition");
		}

		// Create and validate the barrier
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = 0;

		barrier.oldLayout = currentState.layout;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		barrier.srcQueueFamilyIndex = currentState.owningQueueFamily;
		barrier.dstQueueFamilyIndex = targetQueueFamily;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(cmdBuf,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		// Wait with timeout to prevent hanging
		waitPreviousTransition();

		currentState.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		currentState.owningQueueFamily = targetQueueFamily;

		isTransitioning = false;
		return true;
	}

	bool RenderTexture::TransitionForRead(VkCommandBuffer cmdBuf, uint32_t targetQueueFamily) {
		std::lock_guard<std::mutex> lock(stateMutex);

		if (isTransitioning.exchange(true)) {
			throw std::runtime_error("Previous transition still in progress");
		}

		waitPreviousTransition();

		// Validate current state
		if (currentState.layout == VK_IMAGE_LAYOUT_UNDEFINED) {
			// Bind memory to image
			vkBindImageMemory(RenderDevice->getLogicalDevice(), image, imageMemory, 0);

			// Transition from UNDEFINED to TRANSFER_DST_OPTIMAL
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmdBuf,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);
		}

		if (currentState.owningQueueFamily != VK_QUEUE_FAMILY_IGNORED &&
			currentState.owningQueueFamily != targetQueueFamily) {
			throw std::runtime_error("Invalid queue family transition");
		}

		// Create and validate the barrier
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = 0;

		barrier.oldLayout = currentState.layout;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		barrier.srcQueueFamilyIndex = currentState.owningQueueFamily;
		barrier.dstQueueFamilyIndex = targetQueueFamily;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(cmdBuf,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		// Wait with timeout to prevent hanging
		waitPreviousTransition();

		currentState.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		currentState.owningQueueFamily = targetQueueFamily;

		isTransitioning = false;
		return true;
	}

	void RenderTexture::waitPreviousTransition() {
		if (currentState.lastTransitionFence != VK_NULL_HANDLE) {
			vkWaitForFences(RenderDevice->getLogicalDevice(),
				1, &currentState.lastTransitionFence,
				VK_TRUE, 100);
			vkResetFences(RenderDevice->getLogicalDevice(),
				1, &currentState.lastTransitionFence);
		}
	}

	VkExtent2D RenderTexture::getExtent() {
		return VkExtent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	}

	void RenderTexture::createDepthResources() {
		depthFormat = findDepthFormat(RenderDevice);

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = depthFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(RenderDevice->getLogicalDevice(), &imageInfo, nullptr, &depthImage) != VK_SUCCESS)
			throw std::runtime_error("failed to create depth image!");

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(RenderDevice->getLogicalDevice(), depthImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = RenderDevice->findMemoryType(memRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(RenderDevice->getLogicalDevice(), &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate depth image memory!");

		vkBindImageMemory(RenderDevice->getLogicalDevice(), depthImage, depthImageMemory, 0);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = depthImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = depthFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(RenderDevice->getLogicalDevice(), &viewInfo, nullptr, &depthImageView) != VK_SUCCESS)
			throw std::runtime_error("failed to create depth image view!");
	}
}