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
        std::vector<RenderTexture*> colorAttachments;
        DepthBuffer* depthTexture = nullptr;

        bool IsSwapchain() const { return swapchain != nullptr; }
        bool IsTexture()   const { return !colorAttachments.empty(); }

        bool IsValid() const {
            // Valid if it's either a swapchain OR has at least one color texture
            return (swapchain != nullptr) ^ (!colorAttachments.empty());
        }


        void AddTexture(RenderTexture* tex) {
            colorAttachments.push_back(tex);
        }

        RenderTexture* GetTexture(int index) const {
            if (index < 0 || index >= colorAttachments.size()) return nullptr;
            return colorAttachments[index];
        }

        uint32_t GetColorAttachmentCount() const {
            if (IsSwapchain()) return 1;
            return static_cast<uint32_t>(colorAttachments.size());
        }


        VkFormat GetFormat(uint32_t index = 0) const {
            if (IsSwapchain()) return swapchain->GetSwapChainImageFormat();
            if (index < colorAttachments.size()) return colorAttachments[index]->getFormat();
            return VK_FORMAT_UNDEFINED;
        }

        VkExtent2D GetExtent() const {
            if (IsSwapchain()) return swapchain->GetSwapChainExtent();
            if (!colorAttachments.empty()) return colorAttachments[0]->getExtent();
            return { 0, 0 };
        }

        std::vector<VkImageView> GetAttachmentViews(uint32_t currentFrame = 0) const {
            std::vector<VkImageView> views;
            if (IsSwapchain()) {
                views.push_back(swapchain->getImageViews()[currentFrame]);
            }
            else {
                for (auto* tex : colorAttachments) {
                    views.push_back(tex->GetImageView());
                }
            }
            if (HasDepth()) views.push_back(GetDepthImageView());
            return views;
        }

        VkAttachmentDescription GetColorAttachmentDescription(
            uint32_t index,
            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE) const
        {
            VkAttachmentDescription desc{};
            desc.format = GetFormat(index);
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = loadOp;
            desc.storeOp = storeOp;
            desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            desc.finalLayout = finalLayout;
            return desc;
        }

        void TransitionForRead(VkCommandBuffer cmdBuf, uint32_t currentFrame) const {
            for (uint32_t i = 0; i < GetColorAttachmentCount(); ++i) {
                VkImage image = IsSwapchain() ? swapchain->getImages()[currentFrame] : colorAttachments[i]->GetImage();

                VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.image = image;
                barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            }
        }

        void TransitionForWrite(VkCommandBuffer cmdBuf, uint32_t currentFrame) const {
            for (uint32_t i = 0; i < GetColorAttachmentCount(); ++i) {
                VkImage image = IsSwapchain() ? swapchain->getImages()[currentFrame] : colorAttachments[i]->GetImage();

                VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.image = image;
                barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            }
        }

        VkAttachmentDescription GetDepthAttachmentDescription() const {
            VkAttachmentDescription depthAttachment{};
            if (!HasDepth()) return depthAttachment;

            depthAttachment.format = GetDepthFormat();
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // store the depth buffer just in case
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            return depthAttachment;
        }

        bool HasDepth() const { return depthTexture != nullptr; }
        VkFormat GetDepthFormat() const { return depthTexture ? depthTexture->depthFormat : VK_FORMAT_UNDEFINED; }
        VkImageView GetDepthImageView() const { return depthTexture ? depthTexture->depthImageView : VK_NULL_HANDLE; }
    };
}