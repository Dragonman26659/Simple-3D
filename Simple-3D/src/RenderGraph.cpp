#pragma once
#include "Internal/RenderGraph.h"



namespace Simple3D {
    void RenderGraph::Execute(VkCommandBuffer cmd, RenderData data, uint32_t currentFrame) {
        for (auto* pass : executionOrder) {
            pass->Execute(cmd, data, currentFrame);
        }
    }



    // Compile rendergraph to correct order
    void RenderGraph::Compile() {
        executionOrder.clear();

        // Naive topological sort based on inputs/outputs
        std::unordered_set<std::string> produced;

        while (executionOrder.size() < passes.size()) {
            bool progress = false;

            for (auto& pass : passes) {
                if (std::find(executionOrder.begin(), executionOrder.end(), pass.get()) != executionOrder.end())
                    continue;

                bool ready = true;
                for (const auto& input : pass->inputResources) {
                    if (!produced.count(input) && !resources[input].external) {
                        ready = false;
                        break;
                    }
                }

                if (ready) {
                    executionOrder.push_back(pass.get());
                    for (const auto& out : pass->outputResources)
                        produced.insert(out);
                    progress = true;
                }
            }

            if (!progress)
                throw std::runtime_error("RenderGraph cyclic dependency detected!");
        }
    }

    std::vector<VkFramebuffer> RenderGraph::CreateFramebufferForTarget(
        Device& device,
        const RenderTarget& target,
        VkRenderPass renderPass)
    {
        std::vector<VkFramebuffer> framebuffers;
        std::vector<VkImageView> attachments;
        VkExtent2D extent{};

        if (target.texture) {
            // Offscreen render target (single framebuffer)
            attachments.clear();
            attachments.push_back(target.texture->GetImageView());
            if (target.texture->getDepthView() != VK_NULL_HANDLE)
                attachments.push_back(target.texture->getDepthView());

            extent = target.texture->getExtent();

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = renderPass;
            fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            fbInfo.pAttachments = attachments.data();
            fbInfo.width = extent.width;
            fbInfo.height = extent.height;
            fbInfo.layers = 1;

            VkFramebuffer framebuffer;
            if (vkCreateFramebuffer(device.getLogicalDevice(), &fbInfo, nullptr, &framebuffer) != VK_SUCCESS)
                throw std::runtime_error("Failed to create framebuffer for RenderTexture target!");

            framebuffers.push_back(framebuffer);
        }
        else if (target.swapchain) {
            const auto& views = target.swapchain->getImageViews();
            extent = target.swapchain->GetSwapChainExtent();

            // one framebuffer per swapchain image
            framebuffers.reserve(views.size());
            for (const auto& view : views) {
                attachments.clear();
                attachments.push_back(view);


                // Add Depth Through Swapchain
                //if (target.swapchain->GetDepthImageView() != VK_NULL_HANDLE)
                //    attachments.push_back(target.swapchain->GetDepthImageView());

                VkFramebufferCreateInfo fbInfo{};
                fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fbInfo.renderPass = renderPass;
                fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                fbInfo.pAttachments = attachments.data();
                fbInfo.width = extent.width;
                fbInfo.height = extent.height;
                fbInfo.layers = 1;

                VkFramebuffer framebuffer;
                if (vkCreateFramebuffer(device.getLogicalDevice(), &fbInfo, nullptr, &framebuffer) != VK_SUCCESS)
                    throw std::runtime_error("Failed to create framebuffer for swapchain image!");

                framebuffers.push_back(framebuffer);
            }
        }
        else {
            throw std::runtime_error("RenderTarget is invalid, cannot create framebuffer!");
        }

        return framebuffers;
    }


    void RenderGraph::Build(Device& device, const std::unordered_map<PassType, VkRenderPass>& passLayouts)
    {
        for (auto& passPtr : passes)
        {
            auto& pass = *passPtr;
            RenderInfo& info = pass.renderInfo;

            if (!info.target.IsValid())
                continue;

            // Choose render pass layout based on pass type
            VkRenderPass renderPass = passLayouts.at(pass.type);

            // Create framebuffer for this target
            info.framebuffers = CreateFramebufferForTarget(device, info.target, renderPass);
            info.framebuffer = info.framebuffers[0];
            info.renderPass = renderPass;
        }
    }

}