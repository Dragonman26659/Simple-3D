#pragma once
// Standard library headers
#include <mutex>
#include <atomic>
#include <stdexcept>

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
		VkFormat format;

		TextureBinding* binding = nullptr;


		// Create image view

		void createRenderPassForImageView();

		void createImageView();

		void createImageForImageView();

		void createFramebufferForImageView(int width, int height);


		void setupTransitionResources() {
			// Create fence for synchronization
			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			currentState.owningQueueFamily = VK_QUEUE_FAMILY_IGNORED;
			vkCreateFence(RenderDevice->getLogicalDevice(), &fenceInfo, nullptr, &currentState.lastTransitionFence);
			vkResetFences(RenderDevice->getLogicalDevice(), 1, &currentState.lastTransitionFence);
		}

	public:
		struct ImageState {
			VkImageLayout layout;
			uint32_t owningQueueFamily;
			VkFence lastTransitionFence;
		};

		ImageState currentState;
		std::mutex stateMutex;
		std::atomic<bool> isTransitioning = false;

		int width, height;



		RenderTexture(Device* RenderDevice, int width, int height, VkFormat format);
		RenderTexture();
		~RenderTexture();
		void cleanup();
		bool resize(int width, int height);
		VkExtent2D getExtent();


		TextureBinding* getBinding();

		bool TransitionForWrite(VkCommandBuffer cmdBuf, uint32_t targetQueueFamily);
		bool TransitionForRead(VkCommandBuffer cmdBuf, uint32_t targetQueueFamily);
		void waitPreviousTransition();


		VkImageView GetImageView() { return imageView; }
		VkRenderPass getRenderPass() { return renderPass;  }
		VkFramebuffer getFrameBuffer() { return framebuffer; }
	};
}