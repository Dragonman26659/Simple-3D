#pragma once
#include "SimpleCore.h"
#include "Tools.h"
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Component/Tools/Material.h"


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

		TextureBinding* binding = nullptr;

		VkFence transitionFence = VK_NULL_HANDLE;


		// Create image view

		void createRenderPassForImageView();

		void createImageView();

		void createImageForImageView();

		void createFramebufferForImageView(int width, int height);


		void setupTransitionResources() {
			// Create fence for synchronization
			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			vkCreateFence(RenderDevice->getLogicalDevice(), &fenceInfo, nullptr, &transitionFence);
		}

		int width, height;

	public:
		RenderTexture(Device* RenderDevice, SwapChain* swapChain);
		RenderTexture();
		~RenderTexture();
		void cleanup();
		bool resize(int width, int height);


		TextureBinding* getBinding();

		bool transitionToPresentSrcKHR(VkCommandPool* cmdPool);
		bool transitionToShaderReadOptimal(VkCommandPool* cmdPool);


		VkImageView GetImageView() { return imageView; }
		VkRenderPass getRenderPass() { return renderPass;  }
		VkFramebuffer getFrameBuffer() { return framebuffer; }
	};
}