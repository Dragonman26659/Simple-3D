#pragma once
#include "Internal/RenderGraph.h"



namespace Simple3D {
    RenderGraph::RenderGraph(std::string name, VkCommandPool& commandPool, Device& device) : name(name), commandPool(commandPool) {
    }



    void RenderGraph::Execute(VkCommandBuffer cmd, RenderData data, uint32_t currentFrame) {
        for (auto* pass : executionOrder) {
            pass->Execute(cmd, data, currentFrame);
        }
    }


    RenderGraph::~RenderGraph() {
        for (RenderPass* pass : passes) {
            delete pass;
        }
    }

    

    // --- Generate: create renderpasses and then compile+build ---
    void RenderGraph::Generate(Device& device, std::vector<ShaderSet*> shaders)
    {
        printf("Compiling shaders...\n");


        // Compile the graph into execution order
        Compile();


        // Build (create framebuffers, allocate depth textures, etc.)
        Build(device, shaders);
    }






    // Compile rendergraph to correct order
    void RenderGraph::Compile() {
        executionOrder.clear();

        // Naive topological sort based on inputs/outputs
        std::unordered_set<std::string> produced;

        while (executionOrder.size() < passes.size()) {
            bool progress = false;

            for (auto& pass : passes) {
                if (std::find(executionOrder.begin(), executionOrder.end(), pass) != executionOrder.end())
                    continue;

                bool ready = true;
                for (const auto& input : pass->inputResources) {
                    if (!produced.count(input) && !resources[input].external) {
                        ready = false;
                        break;
                    }
                }

                if (ready) {
                    if (pass->outputResources.empty()) {
                        printf("Renderpass: %s doesn't produce an output\n", pass->name.c_str());
                        continue;
                    }

                    executionOrder.push_back(pass);
                    for (const auto& out : pass->outputResources)
                        produced.insert(out);
                    progress = true;
                }
            }

            if (!progress) {
                printf("RenderGraph cyclic dependency detected!");
                throw std::runtime_error("RenderGraph cyclic dependency detected!");
            }
        }
    }
    std::vector<VkFramebuffer> RenderGraph::CreateFramebufferForTarget(
        Device& device,
        RenderTarget& target,
        VkRenderPass renderPass)
    {
        std::vector<VkFramebuffer> framebuffers;
        VkExtent2D extent{};

        if (target.texture) {
            // --- Offscreen render target (single framebuffer) ---
            std::vector<VkImageView> attachments;
            attachments.push_back(target.texture->GetImageView()); // Color

            if (target.HasDepth())
                attachments.push_back(target.depthTexture->depthImageView); // Depth

            extent = target.texture->getExtent();

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = renderPass;
            fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            fbInfo.pAttachments = attachments.data();
            fbInfo.width = extent.width;
            fbInfo.height = extent.height;
            fbInfo.layers = 1;

            VkFramebuffer framebuffer{};
            if (vkCreateFramebuffer(device.getLogicalDevice(), &fbInfo, nullptr, &framebuffer) != VK_SUCCESS)
                throw std::runtime_error("Failed to create framebuffer for RenderTexture target!");

            framebuffers.push_back(framebuffer);
        }
        else if (target.swapchain) {
            // --- Swapchain render target (one framebuffer per swapchain image) ---
            const auto& views = target.swapchain->getImageViews();
            extent = target.swapchain->GetSwapChainExtent();

            framebuffers.reserve(views.size());

            for (const auto& colorView : views) {
                std::vector<VkImageView> attachments;
                attachments.push_back(colorView);

                if (target.HasDepth()) {
                    // handle if resize
                    if (target.depthTexture->extent.width != target.GetExtent().width
                        || target.depthTexture->extent.height != target.GetExtent().height)
                        delete target.depthTexture;
                        target.depthTexture = new DepthBuffer(&device, target.GetExtent(), &commandPool);


                    attachments.push_back(target.depthTexture->depthImageView);
                }

                VkFramebufferCreateInfo fbInfo{};
                fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fbInfo.renderPass = renderPass;
                fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                fbInfo.pAttachments = attachments.data();
                fbInfo.width = extent.width;
                fbInfo.height = extent.height;
                fbInfo.layers = 1;

                VkFramebuffer framebuffer{};
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


    void RenderGraph::Build(Device& device, std::vector<ShaderSet*> shaders)
    {
        for (auto& passPtr : passes)
        {
            auto& pass = *passPtr;
            RenderInfo& info = pass.renderInfo;


            pass.device = &device;
            pass.commandPool = &commandPool;

            if (!pass.outputResources.empty()) {
                pass.renderInfo.target = *resources[pass.outputResources[0]].target;
            }

            for (auto& inputName : pass.inputResources)
            {
                auto& res = resources[inputName];

                if (!res.target->IsTexture()) {
                    printf("Input has no corrosponding texture, skipping...\n");
                    continue;
                }

                pass.BoundResources[inputName] = res.target->texture->getBinding()->descriptorSet;
            }

            
            pass.GenerateRenderPass();
            pass.CreatePipelines(shaders);

            if (!info.target.IsValid())
                continue;

            // Create framebuffer for this target
            info.framebuffers = CreateFramebufferForTarget(device, info.target, passPtr->renderPass);
            info.framebuffer = info.framebuffers[0];
        }
    }

    void RenderGraph::RegenerateFramebuffers(Device& device) {
        for (auto& passPtr : passes)
        {
            auto& pass = *passPtr;
            RenderInfo& info = pass.renderInfo;

            if (info.target.swapchain) {
                // Create framebuffer for this target
                info.framebuffers = CreateFramebufferForTarget(device, info.target, passPtr->renderPass);
                info.framebuffer = info.framebuffers[0];
            }
        }
    }

    void RenderPass::GenerateRenderPass() {
        const auto& target = renderInfo.target;
        if (!target.IsValid())
            throw std::runtime_error("RenderPass::GenerateRenderPass() -> Invalid RenderTarget!");


        SetUpPass();

        bool hasDepth = target.HasDepth();

        // --- Build attachments using helper methods ---
        std::vector<VkAttachmentDescription> attachments;
        attachments.push_back(target.GetColorAttachmentDescription(
             target.IsSwapchain() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        ));

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        if (hasDepth) {
            attachments.push_back(target.GetDepthAttachmentDescription());
            depthRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
            depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        // --- Subpass ---
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;

        // --- Subpass Dependencies (handle layout transitions & sync) ---
        std::array<VkSubpassDependency, 2> dependencies{};

        // External -> Subpass
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Subpass -> External
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // --- Create the Render Pass ---
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(device->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
            throw std::runtime_error("RenderPass::GenerateRenderPass() -> Failed to create VkRenderPass!");

        // Assign debug name
        SetObjectName(
            device->getLogicalDevice(),
            reinterpret_cast<uint64_t>(renderPass),
            VK_OBJECT_TYPE_RENDER_PASS,
            name
        );
    }



    void RenderPass::CreatePipelines(std::vector<ShaderSet*> shaders) {
        if (renderPass == VK_NULL_HANDLE) return;

        for (ShaderSet* shaderset : shaders) {
            renderInfo.Pipelines[shaderset] = new Pipeline(*device, *shaderset, renderPass, renderInfo.target, PassConfig);
        }
    }
}