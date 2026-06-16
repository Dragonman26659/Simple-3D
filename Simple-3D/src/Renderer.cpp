#include "Renderer.h"

// Helper function for error messages
std::string vkResultToString(VkResult result) {
	switch (result) {
	case VK_SUCCESS: return "VK_SUCCESS";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
	default: return "Unknown error";
	}
}


namespace Simple3D {
	// Actual render loop :0
	void Renderer::Render(RenderData& data) {
		if (isMinimised()) {
			return;
		}


		// Wait for last frame to complete
		vkWaitForFences(RenderDevice->getLogicalDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
		vkResetFences(RenderDevice->getLogicalDevice(), 1, &inFlightFences[currentFrame]);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(RenderDevice->getLogicalDevice(),
			swapChain->GetVKSwapchain(),
			UINT64_MAX,
			imageAvailableSemaphores[currentFrame],
			VK_NULL_HANDLE,
			&imageIndex);


		// Handle Window Resizing
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || WindowResized()) {
			WindoResize();
		}
		else if (result != VK_SUCCESS) {
			printf("Simple3D failed to acquire swap chain image!");
			throw std::runtime_error("Simple3D failed to acquire swap chain image!");
		}

		// Reset command buffer after fence reset
		vkResetCommandBuffer(commandBuffers[currentFrame], 0);

		// Record the Command buffer
		recordCommandBuffer(commandBuffers[currentFrame], currentFrame, imageIndex, data);


		// Submit the frame to screen
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;


		VkResult submitResult = vkQueueSubmit(RenderDevice->getVKgraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]);

		if (submitResult != VK_SUCCESS) {
			std::string errorMessage = "Failed to submit draw command buffer! (Error code: " +
				std::to_string(submitResult) + ")";
			
			printf(errorMessage.c_str());
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		// Present
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain->GetVKSwapchain();
		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(RenderDevice->getVKpresentQueue(), &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}



		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}


	void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t syncIndex, uint32_t imageIndex, RenderData& data) {

		// Begin to record
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional 
		beginInfo.pInheritanceInfo = nullptr; // Optional



		if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
			printf("failed to begin recording command buffer!");
			throw std::runtime_error("failed to begin recording command buffer!");
		}


		// Clear Screen
		VkRenderPassBeginInfo clearRenderPassInfo = {};
		clearRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		clearRenderPassInfo.renderPass = renderPass;
		clearRenderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
		clearRenderPassInfo.renderArea.offset = { 0, 0 };
		clearRenderPassInfo.renderArea.extent = swapChain->GetSwapChainExtent();

		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };  // Black clear color
		clearValues[1].depthStencil = { 1.0f, 0 };
		clearRenderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		clearRenderPassInfo.pClearValues = clearValues.data();

		bool haveRendered = false;



		// Render All Render to texture instances
		for (RenderGraph* graph : RenderGraphs) {
			graph->Execute(cmd, data, imageIndex, syncIndex);
			haveRendered = true;
		}

		if (!haveRendered) {
			// Get render pass and its information
			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
			renderPassInfo.renderPass = renderPass;
			

			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChain->GetSwapChainExtent();
			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassInfo.pClearValues = clearValues.data();


			// Clear the screen
			vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdEndRenderPass(cmd);
		}



// TODO MAKE THIS A PASS IN RENDERGRAPH
#ifdef USEIMGUI
		if (usingImgui) {
			

			// First barrier: Transition to SHADER_READ_ONLY_OPTIMAL
			VkImageMemoryBarrier barrier1{};
			barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier1.image = swapChain->getImages()[imageIndex];
			barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier1.subresourceRange.baseMipLevel = 0;
			barrier1.subresourceRange.levelCount = 1;
			barrier1.subresourceRange.baseArrayLayer = 0;
			barrier1.subresourceRange.layerCount = 1;
			barrier1.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier1.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			barrier1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			VkPipelineStageFlags srcStageMask1 = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			VkPipelineStageFlags dstStageMask1 = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			vkCmdPipelineBarrier(cmd,
				srcStageMask1,
				dstStageMask1,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier1);

			// Begin imgui render pass
			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = ClearRenderPass;
			renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChain->GetSwapChainExtent();
			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassInfo.pClearValues = clearValues.data();

			// Second barrier: Transition to PRESENT_SRC_KHR after imgui
			VkImageMemoryBarrier barrier2{};
			barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier2.image = swapChain->getImages()[imageIndex];
			barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier2.subresourceRange.baseMipLevel = 0;
			barrier2.subresourceRange.levelCount = 1;
			barrier2.subresourceRange.baseArrayLayer = 0;
			barrier2.subresourceRange.layerCount = 1;
			barrier2.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier2.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier2.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

			vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			drawImgui(cmd, imageIndex);
			vkCmdEndRenderPass(cmd);

			VkPipelineStageFlags srcStageMask2 = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			VkPipelineStageFlags dstStageMask2 = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

			vkCmdPipelineBarrier(cmd,
				srcStageMask2,
				dstStageMask2,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier2);
		}
#endif // USEIMGUI


		if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
			printf("failed to record command buffer!");
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	// Creates a material given a material create struct, Material memory is handled by renderer
	Material* Renderer::CreateMaterial(MaterialInfo info) {
		Material* material = new Material(RenderDevice, &commandPool,info.textures, info.shaders);
		material->updateSortedTextureNames();
		return material;
	}


	ShaderSet* Renderer::CreateShaderSet(std::string name) {
		ShaderSet* set = new ShaderSet(RenderDevice->getLogicalDevice(), name);
		shaders.push_back(set);
		return set;
	}

	void Renderer::BuildRenderGraphs() {
		// Make sure all shaders have been reflected
		for (ShaderSet* set : shaders) {
			if (!set->reflected)
				set->Reflect();
		}

		// Build rendergraphs
		for (RenderGraph* graph : RenderGraphs) {
			graph->Generate(*RenderDevice, shaders);
		}
	}

	void Renderer::BuildGraph(RenderGraph* graph) {
		// Make sure all shaders have been reflected
		for (ShaderSet* set : shaders) {
			if (!set->reflected)
				set->Reflect();
		}

		graph->Generate(*RenderDevice, shaders);
	}


	RenderGraph* Renderer::CreateRenderGraph(std::string name) {
		RenderGraph* graph = new RenderGraph(name, commandPool, *RenderDevice);
		RenderGraphs.push_back(graph);
		return graph;
	}




	RenderTexture* Renderer::CreateRenderTexture(int width, int height) {
		RenderTexture* texture = new RenderTexture(RenderDevice, width, height, swapChain->GetSwapChainImageFormat());
		RenderTextures.push_back(texture);
		return texture;
	}
	RenderTexture* Renderer::CreateRenderTexture(int width, int height, VkFormat format) {
		RenderTexture* texture = new RenderTexture(RenderDevice, width, height, format);
		RenderTextures.push_back(texture);
		return texture;
	}


	void Renderer::WindoResize() {
		RecreateSwapChain();

		for (RenderGraph* graph : RenderGraphs) {
			graph->RegenerateFramebuffers(*RenderDevice);
		}

	}

	TextureBinding Renderer::CreateTexture(std::string filepath) {
		return CreateTextureBinding(filepath, RenderDevice, &commandPool);
	}

	TextureCube* Renderer::CreateTextureCube(std::vector<std::string> filepath) {
		return new TextureCube(*RenderDevice, filepath, true, &commandPool);
	}

	DepthBuffer* Renderer::CreateDepth(RenderTarget target) {
		return new DepthBuffer(RenderDevice, target.GetExtent(), &commandPool);
	}

	DepthBuffer* Renderer::CreateDepth(VkExtent2D extent) {
		return new DepthBuffer(RenderDevice, extent, &commandPool);
	}

	//////////////////////////////////////////
	//				VULKAN SHITE			//
	//////////////////////////////////////////



		// Debug Messenger
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	// Destructor
	Renderer::~Renderer() {
		// Wait for GPU to finish all operations
		vkDeviceWaitIdle(RenderDevice->getLogicalDevice());

#ifdef USEIMGUI
		if (usingImgui) {
			// Delete Imgui
			ImGui_ImplVulkan_Shutdown();
#ifdef SDL_WINDOW
			ImGui_ImplSDL2_Shutdown();
#else
			ImGui_ImplGlfw_Shutdown();
#endif
			ImGui::DestroyContext();
		}
#endif // USEIMGUI

		// Cleanup synchronization objects
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(RenderDevice->getLogicalDevice(), renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(RenderDevice->getLogicalDevice(), imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(RenderDevice->getLogicalDevice(), inFlightFences[i], nullptr);
		}

		// Cleanup command pool
		vkDestroyCommandPool(RenderDevice->getLogicalDevice(), commandPool, nullptr);

		// Cleanup framebuffers
		for (auto framebuffer : swapChainFramebuffers) {
			vkDestroyFramebuffer(RenderDevice->getLogicalDevice(), framebuffer, nullptr);
		}

		// Cleanup render pass
		vkDestroyRenderPass(RenderDevice->getLogicalDevice(), renderPass, nullptr);

		// Cleanup swap chain
		delete swapChain;

		// Cleanup device
		delete RenderDevice;

		// Cleanup surface
		vkDestroySurfaceKHR(instance, surface, nullptr);

		// Cleanup debug messenger
		if (enableValidationLayers && VALIDATION_LAYERS_ENABLED) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		// Cleanup instance
		vkDestroyInstance(instance, nullptr);
	}
	void Renderer::CreateInstance(std::string EngineName, std::string ApplicationName) {
		// Store strings locally to ensure lifetime
		std::string engineStr = EngineName;
		std::string appName = ApplicationName;

		// App information
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = appName.c_str();
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = engineStr.c_str();
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		// Create the instance
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// Handle extensions
		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		// Validation layers -- Checking for support
		if (enableValidationLayers && VALIDATION_LAYERS_ENABLED && !checkValidationLayerSupport()) {
			printf("validation layers requested, but not available!\n");
			//enableValidationLayers = false;
			throw std::runtime_error("validation layers requested, but not available!\n");
		}

		// Handle validation layers
		if (enableValidationLayers && VALIDATION_LAYERS_ENABLED) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		// Create instance with comprehensive error handling
		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
		if (result != VK_SUCCESS) {
			std::string errorString = vkResultToString(result);
			printf("Error: Failed to create Vulkan instance (error code: %s)\n", errorString.c_str());
			throw std::runtime_error("failed to create instance: " + errorString);
		}
	}

	bool Renderer::checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	std::vector<const char*> Renderer::getRequiredExtensions() {

		std::vector<const char*> extensions(WindowExtensions, WindowExtensions + WindowExtensionCount);

		if (enableValidationLayers && VALIDATION_LAYERS_ENABLED) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		

		return extensions;
	}

	void Renderer::setupDebugMessenger() {
		if (!enableValidationLayers && VALIDATION_LAYERS_ENABLED) return;
	
		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
		createInfo.pUserData = nullptr;

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}

	void Renderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}


	void Renderer::CreateRenderPass() {
		// --- Color attachment (no depth) ---
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChain->GetSwapChainImageFormat();
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// --- Attachment reference ---
		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// --- Subpass ---
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = nullptr; // no depth

		// --- Subpass dependency ---
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// --- Main ImGui render pass ---
		std::array<VkAttachmentDescription, 1> attachments = { colorAttachment };

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(RenderDevice->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create ImGui render pass!");
		}

		// --- Clear (load) version of ImGui render pass ---
		VkAttachmentDescription colorAttachmentClear = colorAttachment;
		colorAttachmentClear.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		colorAttachmentClear.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachmentClear.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		attachments = { colorAttachmentClear };

		VkRenderPassCreateInfo renderPassInfoClear = renderPassInfo;
		renderPassInfoClear.pAttachments = attachments.data();

		if (vkCreateRenderPass(RenderDevice->getLogicalDevice(), &renderPassInfoClear, nullptr, &ClearRenderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create ImGui clear render pass!");
		}
	}

	void Renderer::createFramebuffers() {
		swapChainFramebuffers.resize(swapChain->getImageViews().size());

		for (size_t i = 0; i < swapChain->getImageViews().size(); i++) {
			std::array<VkImageView, 1> attachments = {
				swapChain->getImageViews()[i],
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChain->GetSwapChainExtent().width;
			framebufferInfo.height = swapChain->GetSwapChainExtent().height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(RenderDevice->getLogicalDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	void Renderer::createCommandPool() {
		QueueFamilyIndices queueFamilyIndices = RenderDevice->findQueueFamilies();

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();


		if (vkCreateCommandPool(RenderDevice->getLogicalDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void Renderer::createCommandBuffer() {
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);


		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (vkAllocateCommandBuffers(RenderDevice->getLogicalDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void Renderer::createSyncObjects() {
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(RenderDevice->getLogicalDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(RenderDevice->getLogicalDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(RenderDevice->getLogicalDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
		}
	}

	void Renderer::RecreateSwapChain() {
		// Wait until device can be changed
		vkDeviceWaitIdle(RenderDevice->getLogicalDevice());



		int width = GetWindowWidth();
		int height = GetWindowHeight();

		cleanupSwapChain();
		swapChain->recreate(width, height);


		createFramebuffers();
	}

	void Renderer::cleanupSwapChain() {
		for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(RenderDevice->getLogicalDevice(), swapChainFramebuffers[i], nullptr);
		}

		swapChain->cleanup();
	}

	void Renderer::WaitToFinish() {
		vkDeviceWaitIdle(RenderDevice->getLogicalDevice());
	}
}