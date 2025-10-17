#pragma once
#include "SimpleCore.h"
#include "Device.h"
#include "Tools.h"



namespace Simple3D {
	class DepthBuffer {
	public:
		DepthBuffer(Device* device, VkExtent2D extent, VkCommandPool* commandPool);
		~DepthBuffer();



		VkImageView depthImageView;
		VkFormat depthFormat;
		VkExtent2D extent;
	private:
		VkImage depthImage;
		VkDeviceMemory depthImageMemory;

		Device* device;
	};
}