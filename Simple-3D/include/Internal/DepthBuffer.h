#pragma once
#include "SimpleCore.h"
#include "Device.h"
#include "Tools.h"
namespace Simple3D {
	class DepthBuffer {
	public:
		DepthBuffer(Device* device, VkExtent2D extent, VkCommandPool* commandPool, uint32_t layerCount = 1);
		~DepthBuffer();
		VkImageView depthImageView;
		VkFormat depthFormat;
		VkImage depthImage;
		VkExtent2D extent;

		VkImageLayout currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	private:
		VkDeviceMemory depthImageMemory;
		uint32_t layerCount;
		Device* device;
	};
}