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
	void Renderer::Render() {
		// Wait for last frame and reset fences
		vkWaitForFences(RenderDevice->getLogicalDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(RenderDevice->getLogicalDevice(), swapChain->GetVKSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		// Only reset the fence if we are submitting work
		vkResetFences(RenderDevice->getLogicalDevice(), 1, &inFlightFences[currentFrame]);

		// Reset and record command buffer
		vkResetCommandBuffer(commandBuffers[currentFrame], 0); 
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex); 


		// Submit command buffer
		VkSubmitInfo submitInfo{}; 
		
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

		if (vkQueueSubmit(RenderDevice->getVKgraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{}; 
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR; 

		presentInfo.waitSemaphoreCount = 1; 
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain->GetVKSwapchain()};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // Technically not needed but who cares

		result = vkQueuePresentKHR(RenderDevice->getVKpresentQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			RecreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		// Change current frame
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;


		// Reset the vector of models
		ModelsThisFrame.clear();
	}




	// Command buffer recording
	void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} }; // Make this controlled by a variable eventually

		// Begin to record
		VkCommandBufferBeginInfo beginInfo{}; 
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; 
		beginInfo.flags = 0; // Optional 
		beginInfo.pInheritanceInfo = nullptr; // Optional 

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) { 
			printf("failed to begin recording command buffer!");
			throw std::runtime_error("failed to begin recording command buffer!"); 
		}


		// Get render pass and its information
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChain->GetSwapChainExtent();
		renderPassInfo.clearValueCount = 1; 
		renderPassInfo.pClearValues = &clearColor; 


		// Begin render pass
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			// Bind pipeline for render pass
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());



			// Define viewport
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(swapChain->GetSwapChainExtent().width);
			viewport.height = static_cast<float>(swapChain->GetSwapChainExtent().height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			// Define sissor
			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = swapChain->GetSwapChainExtent();
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


			// Draw Models
			for (Model* model : ModelsThisFrame) {
				if (!model->hasBuffer()) {
					model->CreateBuffers(RenderDevice, &commandPool);
				}

				// UPdate with models position and shizzle
				pipeline->updateUniformBuffer(currentFrame, mainCamera->getProjectionMatrix(GetWindowWidth(), GetWindowHeight()), mainCamera->getViewMatrix(), model->GetTransform());

				VkBuffer vertexBuffers[] = { model->GetVertexBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

				vkCmdBindIndexBuffer(commandBuffer, model->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);

				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(), 0, 1, &pipeline->descriptorSets[currentFrame], 0, nullptr);

				vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(model->Indices.size()), 1, 0, 0, 0);
			}



		// End render pass
		vkCmdEndRenderPass(commandBuffer); 

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) { 
			printf("failed to record command buffer!");
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	void Renderer::SumbitModelToFrame(Model* model) {
		ModelsThisFrame.push_back(model);
	}

	void Renderer::SubmitMainCamera(Camera* cam) {
		mainCamera = cam;
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
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(RenderDevice->getLogicalDevice(), renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(RenderDevice->getLogicalDevice(), imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(RenderDevice->getLogicalDevice(), inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(RenderDevice->getLogicalDevice(), commandPool, nullptr);

		// Delte framebuffers
		for (auto framebuffer : swapChainFramebuffers) {
			vkDestroyFramebuffer(RenderDevice->getLogicalDevice(), framebuffer, nullptr);
		}

		// In order to keep memory saftey must be in this order
		delete pipeline;
		vkDestroyRenderPass(RenderDevice->getLogicalDevice(), renderPass, nullptr);
		delete swapChain;
		delete RenderDevice;
		vkDestroySurfaceKHR(instance, surface, nullptr);


		if (enableValidationLayers) { 
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr); 
		}
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
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Create the instance
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// Handle extensions
		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		// Validation layers -- Checking for support
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			printf("validation layers requested, but not available!\n");
			//enableValidationLayers = false;
			throw std::runtime_error("validation layers requested, but not available!\n");
		}

		// Handle validation layers
		if (enableValidationLayers) {
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

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}



	void Renderer::setupDebugMessenger() {
		if (!enableValidationLayers) return;
	
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
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChain->GetSwapChainImageFormat();
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; 
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;



		VkAttachmentReference colorAttachmentRef{}; 
		colorAttachmentRef.attachment = 0; 
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 



		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;



		VkSubpassDependency dependency{}; 
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL; 
		dependency.dstSubpass = 0; 
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; 
		dependency.srcAccessMask = 0; 
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; 
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


		// Actual renderpass
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1; 
		renderPassInfo.pDependencies = &dependency; 

		if (vkCreateRenderPass(RenderDevice->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			printf("failed to create render pass!");
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void Renderer::createFramebuffers() {
		swapChainFramebuffers.resize(swapChain->getImageViews().size());

		for (size_t i = 0; i < swapChain->getImageViews().size(); i++) {
			VkImageView attachments[] = {
				swapChain->getImageViews()[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
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


	void Renderer::cleanupSwapChain() {
		for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(RenderDevice->getLogicalDevice(), swapChainFramebuffers[i], nullptr);
		}

		swapChain->cleanup();
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

	void Renderer::WaitToFinish() {
		vkDeviceWaitIdle(RenderDevice->getLogicalDevice());
	}
}