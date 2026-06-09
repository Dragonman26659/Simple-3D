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

        // Shadowmapping Support
        bool isShadowArray = false;
        uint32_t layerCount = 1;

        // --- Identification ---
        bool IsSwapchain() const { return swapchain != nullptr; }
        bool IsTexture()   const { return !colorAttachments.empty(); }
        bool HasDepth()    const { return depthTexture != nullptr; }
        bool IsValid() const {
            // 1. Cannot be both a swapchain and a custom offscreen color texture list
            if (swapchain != nullptr && !colorAttachments.empty()) {
                return false;
            }

            // 2. Must have at least one valid rendering resource attached
            bool hasAnyResource = (swapchain != nullptr) ||
                (!colorAttachments.empty()) ||
                (depthTexture != nullptr);

            return hasAnyResource;
        }

        void AddTexture(RenderTexture* tex) {
            colorAttachments.push_back(tex);
        }

        RenderTexture* GetTexture(int index) const {
            if (index < 0 || index >= colorAttachments.size()) return nullptr;
            return colorAttachments[index];
        }

        // --- Factories ---
        static RenderTarget CreateShadowTarget(DepthBuffer* shadowDepth, uint32_t layers = 1) {
            RenderTarget target{};
            target.depthTexture = shadowDepth;
            target.isShadowArray = (layers > 1);
            target.layerCount = layers;
            return target;
        }

        // --- Getters ---
        VkExtent2D GetExtent() const {
            if (IsSwapchain()) return swapchain->GetSwapChainExtent();
            if (IsTexture())   return colorAttachments[0]->getExtent();
            if (HasDepth())    return depthTexture->extent; // Critical for shadow-only targets
            return { 0, 0 };
        }

        uint32_t GetColorAttachmentCount() const {
            return IsSwapchain() ? 1 : static_cast<uint32_t>(colorAttachments.size());
        }

        VkFormat GetFormat(uint32_t index = 0) const {
            if (IsSwapchain()) return swapchain->GetSwapChainImageFormat();
            if (index < colorAttachments.size()) return colorAttachments[index]->getFormat();
            return VK_FORMAT_UNDEFINED;
        }

        VkFormat    GetDepthFormat()    const { return depthTexture ? depthTexture->depthFormat : VK_FORMAT_UNDEFINED; }
        VkImageView GetDepthImageView() const { return depthTexture ? depthTexture->depthImageView : VK_NULL_HANDLE; }

        VkImageViewType GetDepthViewType() const {
            return isShadowArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        }

        // --- View Collection ---
        std::vector<VkImageView> GetAttachmentViews(uint32_t currentFrame = 0) const {
            std::vector<VkImageView> views;
            if (IsSwapchain()) {
                views.push_back(swapchain->getImageViews()[currentFrame]);
            }
            else {
                for (auto* tex : colorAttachments) views.push_back(tex->GetImageView());
            }
            if (HasDepth()) views.push_back(GetDepthImageView());
            return views;
        }

        // --- Attachment Descriptions ---
        VkAttachmentDescription GetColorAttachmentDescription(uint32_t index,
            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR) const
        {
            VkAttachmentDescription desc{};
            desc.format = GetFormat(index);
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = loadOp;
            desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            desc.finalLayout = finalLayout;
            return desc;
        }

        VkAttachmentDescription GetDepthAttachmentDescription(
            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR) const
        {
            VkAttachmentDescription desc{};
            if (!HasDepth()) return desc;
            desc.format = GetDepthFormat();
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = loadOp;
            desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            desc.finalLayout = finalLayout;
            return desc;
        }

        // --- Transitions ---
        void TransitionForRead(VkCommandBuffer cmd, uint32_t frameIndex) const {
            // Color Transitions
            uint32_t count = GetColorAttachmentCount();
            for (uint32_t i = 0; i < count; ++i) {
                VkImage img = IsSwapchain() ? swapchain->getImages()[frameIndex] : colorAttachments[i]->GetImage();
                TransitionImage(cmd, img, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 1);
            }
            // Depth/Shadow Transition
            if (HasDepth()) {
                TransitionImage(cmd, depthTexture->depthImage, VK_IMAGE_ASPECT_DEPTH_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, layerCount);
            }
        }

        void TransitionForWrite(VkCommandBuffer cmd, uint32_t frameIndex) const {
            // Color Transitions
            uint32_t count = GetColorAttachmentCount();
            for (uint32_t i = 0; i < count; ++i) {
                VkImage img = IsSwapchain() ? swapchain->getImages()[frameIndex] : colorAttachments[i]->GetImage();
                TransitionImage(cmd, img, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 1);
            }
            // Depth/Shadow Transition
            if (HasDepth()) {
                TransitionImage(cmd, depthTexture->depthImage, VK_IMAGE_ASPECT_DEPTH_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, layerCount);
            }
        }

        void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect,
            VkImageLayout oldLayout, VkImageLayout newLayout,
            VkAccessFlags srcAccess, VkAccessFlags dstAccess,
            VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, uint32_t layers) const
        {
            VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcAccessMask = srcAccess;
            barrier.dstAccessMask = dstAccess;
            barrier.image = image;
            barrier.subresourceRange = { aspect, 0, 1, 0, layers };
            vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    };
}