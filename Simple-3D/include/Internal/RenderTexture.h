#pragma once
#include "SimpleCore.h"
#include "Tools.h"
#include "Internal/Device.h"
#include "Internal/SwapChain.h"

namespace Simple3D {
	class RenderTexture {
	private:
		VkDeviceMemory imageMemory;
		VkImageView imageView;
		VkImage image;

		VkFramebuffer framebuffer;
		VkRenderPass renderPass;

		Device* RenderDevice;
		SwapChain* swapChain;


		// Create image view

		void createRenderPassForImageView();

		VkImageView createImageView(VkDevice device, VkImage image, VkFormat format);

		void createImageForImageView();

		void createFramebufferForImageView();


	public:
		RenderTexture(Device* RenderDevice, SwapChain* swapChain);
		RenderTexture();
		~RenderTexture();

		VkImageView GetImageView() { return imageView; }
		VkRenderPass getRenderPass() { return renderPass;  }
		VkFramebuffer getFrameBuffer() { return framebuffer; }
	};
}