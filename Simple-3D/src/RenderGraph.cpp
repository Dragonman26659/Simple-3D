#pragma once
#include "Internal/RenderGraph.h"



namespace Simple3D {
    RenderGraph::RenderGraph(std::string name, VkCommandPool& commandPool, Device& device) : name(name), commandPool(commandPool) {
    }

    void RenderGraph::Execute(VkCommandBuffer cmd, RenderData data, uint32_t currentFrame, uint32_t syncIndex) {
        for (auto* pass : executionOrder) {
            pass->Execute(cmd, data, currentFrame, syncIndex);
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
        VkExtent2D extent = target.GetExtent();
        uint32_t imageCount = target.IsSwapchain() ? static_cast<uint32_t>(target.swapchain->getImageViews().size()) : 1;

        framebuffers.reserve(imageCount);

        for (uint32_t i = 0; i < imageCount; i++) {
            // Automatically gets all color attachments (N textures) and depth if it exists
            std::vector<VkImageView> attachments = target.GetAttachmentViews(i);

            // --- Depth Resizing Logic ---
            if (target.HasDepth()) {
                if (target.depthTexture->extent.width != extent.width ||
                    target.depthTexture->extent.height != extent.height)
                {
                    if (target.depthTexture)
                        delete target.depthTexture;

                    // Note: Ensure your DepthBuffer constructor and commandPool access are correct for your scope
                    target.depthTexture = new DepthBuffer(&device, extent, &commandPool);

                    // Re-fetch views since the depth image view just changed
                    attachments = target.GetAttachmentViews(i);
                }
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
            if (vkCreateFramebuffer(device.getLogicalDevice(), &fbInfo, nullptr, &framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer for render target!");
            }

            framebuffers.push_back(framebuffer);
        }

        return framebuffers;
    }

    void RenderGraph::Build(Device& device, std::vector<ShaderSet*> shaders)
    {
        for (auto& passPtr : passes)
        {
            auto& pass = *passPtr;
            pass.commandPool = &commandPool;
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

                //pass.BoundResources[inputName] = res.target->texture->getBinding()->descriptorSet;
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
        uint32_t colorCount = target.GetColorAttachmentCount();

        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorRefs;

        // --- 1. Build Color Attachments ---
        for (uint32_t i = 0; i < colorCount; ++i) {
            // Determine the final layout: swapchain needs PRESENT, G-Buffers need SHADER_READ
            VkImageLayout finalLayout = target.IsSwapchain()
                ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkAttachmentDescription desc = target.GetColorAttachmentDescription(i, finalLayout);

            if (isOverlay) {
                desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            attachments.push_back(desc);

            VkAttachmentReference ref{};
            ref.attachment = i; // Index in the 'attachments' vector
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorRefs.push_back(ref);
        }

        // --- 2. Build Depth Attachment ---
        VkAttachmentReference depthRef{};
        if (hasDepth) {
            VkImageLayout depthInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

            if (isOverlay) {
                // TransitionForWrite() already moved this image from
                // READ_ONLY_OPTIMAL -> ATTACHMENT_OPTIMAL before the render pass
                // begins. The render pass's own initialLayout has to match that
                // real layout, or LOAD is allowed to discard the existing depth.
                depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                depthInitialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            attachments.push_back(target.GetDepthAttachmentDescription(
                depthInitialLayout,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                depthLoadOp));

            depthRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
            depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        // --- 3. Subpass ---
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.data(); // Points to the array of refs
        subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;

        // --- 4. Subpass Dependencies ---
        // Note: srcAccessMask 0 and dstAccessMask COLOR_WRITE is standard for starting a pass
        std::array<VkSubpassDependency, 2> dependencies{};

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // --- 5. Create the Render Pass ---
        VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(device->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
            throw std::runtime_error("RenderPass::GenerateRenderPass() -> Failed to create VkRenderPass!");

        SetObjectName(device->getLogicalDevice(), reinterpret_cast<uint64_t>(renderPass), VK_OBJECT_TYPE_RENDER_PASS, name);
    }

    void RenderPass::CreatePipelines(std::vector<ShaderSet*> shaders) {
        if (renderPass == VK_NULL_HANDLE) return;

        for (ShaderSet* shaderset : shaders) {
            renderInfo.Pipelines[shaderset] = new Pipeline(*device, *shaderset, renderPass, renderInfo.target, PassConfig);
        }
    }
}