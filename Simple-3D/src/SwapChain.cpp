#include "Internal/SwapChain.h"


namespace Simple3D {
	SwapChain::SwapChain(Device& s_Device, VkSurfaceKHR& surface, int width, int height) 
		: s_Device(s_Device), surface(surface), width(width), height(height) {
		create();
		createImageViews();
	}


	SwapChain::~SwapChain() {
		cleanup();
	}

	// Swap chain operations
	void SwapChain::create() {
		SwapChainSupportDetails swapChainSupport = s_Device.querySwapChainSupport(); 

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats); 
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);  
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities); 


		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) { 
			imageCount = swapChainSupport.capabilities.maxImageCount; 
		}


		VkSwapchainCreateInfoKHR createInfo{}; 
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR; 
		createInfo.surface = surface; 

		createInfo.minImageCount = imageCount; 
		createInfo.imageFormat = surfaceFormat.format; 
		createInfo.imageColorSpace = surfaceFormat.colorSpace; 
		createInfo.imageExtent = extent; 
		createInfo.imageArrayLayers = 1; 
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; 

		QueueFamilyIndices indices = s_Device.findQueueFamilies();
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode; 
		createInfo.clipped = VK_TRUE; 

		createInfo.oldSwapchain = VK_NULL_HANDLE;


		VkResult result = vkCreateSwapchainKHR(s_Device.getLogicalDevice(), &createInfo, nullptr, &swapChain);
		if (result != VK_SUCCESS) {
			printf("failed to create swap chain! error %s", vkResultToString(result));
			throw std::runtime_error("failed to create swap chain!");
		}

		if (swapChain == VK_NULL_HANDLE) {
			printf("swapchain not created!");
			throw std::runtime_error("swapchain not created!");
		}

		vkGetSwapchainImagesKHR(s_Device.getLogicalDevice(), swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount); 
		vkGetSwapchainImagesKHR(s_Device.getLogicalDevice(), swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}


	void SwapChain::recreate(int n_width, int n_height) {
		width = n_width;
		height = n_height;

		create();
		createImageViews();
	}


	void SwapChain::cleanup() {
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			vkDestroyImageView(s_Device.getLogicalDevice(), swapChainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(s_Device.getLogicalDevice(), swapChain, nullptr);
	}

	VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}


	VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}


	VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		else {
			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	void SwapChain::createImageViews() {
		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); i++) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;

			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(s_Device.getLogicalDevice(), &createInfo, nullptr,
				&swapChainImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create texture image view!");
			}
		}
	}

	const std::vector<VkImageView>& SwapChain::getImageViews() const {
		return swapChainImageViews;
	}

	VkFormat& SwapChain::GetSwapChainImageFormat() {
		return swapChainImageFormat;
	}

	VkExtent2D& SwapChain::GetSwapChainExtent() {
		return swapChainExtent;
	}

	VkSwapchainKHR& SwapChain::GetVKSwapchain() {
		return swapChain;
	}
}