#include "Internal/DepthBuffer.h"



namespace Simple3D {
	DepthBuffer::DepthBuffer(Device* device, VkExtent2D extent, VkCommandPool* commandPool) : device(device) {
		VkFormat depthFormat = findDepthFormat(device);

		createImage(extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory, device);
		depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, device);

		transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, device, commandPool);

	}

	DepthBuffer::~DepthBuffer() {
		vkDestroyImageView(device->getLogicalDevice(), depthImageView, nullptr);
		vkDestroyImage(device->getLogicalDevice(), depthImage, nullptr);
		vkFreeMemory(device->getLogicalDevice(), depthImageMemory, nullptr);
	}
}