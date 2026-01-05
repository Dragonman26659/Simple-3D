#pragma once
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Internal/Pipeline.h"
#include "Internal/DepthBuffer.h"
#include "Internal/RenderTexture.h"
#include "Internal/Material/ShaderSet.h"

namespace Simple3D {
	// Target of a renderpass
	struct RenderTarget {
		SwapChain* swapchain = nullptr;
		RenderTexture* texture = nullptr;

		DepthBuffer* depthTexture = nullptr;

		bool IsSwapchain() const { return swapchain != nullptr; }
		bool IsTexture()   const { return texture != nullptr; }

		bool IsValid() const {
			return (swapchain != nullptr) ^ (texture != nullptr);
		}

		VkImage GetVkImage(int currentFrame) const {
			if (texture) return texture->GetImage();
			if (swapchain) return swapchain->getImages()[currentFrame];
			return VK_NULL_HANDLE;
		}


		VkFormat GetFormat() const {
			if (texture) return texture->getFormat();
			if (swapchain) return swapchain->GetSwapChainImageFormat();
			return VK_FORMAT_UNDEFINED;
		}

		VkExtent2D GetExtent() const {
			if (texture) return texture->getExtent();
			if (swapchain) return swapchain->GetSwapChainExtent();
			return { 0, 0 };
		}

		bool HasDepth() const {
			return depthTexture != nullptr;
		}

		VkFormat GetDepthFormat() const {
			if (depthTexture) return depthTexture->depthFormat;
			return VK_FORMAT_UNDEFINED;
		}

		VkImageView GetColorImageView(uint32_t currentFrame = 0) const {
			if (texture) return texture->GetImageView();
			if (swapchain) return swapchain->getImageViews()[currentFrame];
			return VK_NULL_HANDLE;
		}

		VkImageView GetDepthImageView() const {
			return depthTexture ? depthTexture->depthImageView : VK_NULL_HANDLE;
		}

		bool IsOffscreen() const {
			return texture != nullptr && swapchain == nullptr;
		}


		VkAttachmentDescription GetColorAttachmentDescription(
			VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE) const
		{
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = GetFormat();
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = loadOp;
			colorAttachment.storeOp = storeOp;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = finalLayout;
			return colorAttachment;
		}

		VkAttachmentDescription GetDepthAttachmentDescription() const {
			VkAttachmentDescription depthAttachment{};
			if (!HasDepth()) return depthAttachment;

			depthAttachment.format = GetDepthFormat();
			depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			return depthAttachment;
		}

		std::vector<VkImageView> GetAttachmentViews(uint32_t currentFrame = 0) const {
			std::vector<VkImageView> views;
			views.push_back(GetColorImageView(currentFrame));
			if (HasDepth()) views.push_back(GetDepthImageView());
			return views;
		}


		// Redundant for now as it always returns 1 but if later we support many colorattachments per target its needed
		uint32_t GetColorAttachmentCount() const {
			if (texture) {
				// If RenderTexture later supports multiple attachments (e.g. texture array or vector)
				return 1;
			}
			if (swapchain) {
				// Swapchain always has exactly one color attachment
				return 1;
			}
			return 0;
		}

		void TransitionForRead(VkCommandBuffer cmdBuf, uint32_t currentFrame) const {
			VkImage image = GetVkImage(currentFrame);
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(
				cmdBuf,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}


		void TransitionForWrite(VkCommandBuffer cmdBuf, uint32_t currentFrame) const {
			VkImage image = GetVkImage(currentFrame);
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			vkCmdPipelineBarrier(
				cmdBuf,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}
	};
}