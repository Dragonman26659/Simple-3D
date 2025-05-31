#pragma once
#include "SimpleCore.h"
#include "Device.h"
#include "Tools.h"
#include "SwapChain.h"



namespace Simple3D {
	class DepthBuffer {
	public:
		DepthBuffer(Device* device, SwapChain* swapchain, VkCommandPool* commandPool);
		~DepthBuffer();



		VkImageView depthImageView;
	private:
		VkImage depthImage;
		VkDeviceMemory depthImageMemory;

		Device* device;
	};
}