#pragma once
#include "SimpleCore.h"
#include "RenderPass.h"



namespace Simple3D {
    struct RenderResource {
        RenderTarget* target;

        std::string name;
        bool external = false;
    };



    class RenderGraph {
    public:
        std::unordered_map<std::string, RenderResource> resources;
        std::vector<std::unique_ptr<RenderPass>> passes;
        std::vector<RenderPass*> executionOrder;
        std::string name;

        void AddPass(std::unique_ptr<RenderPass> pass) {
            passes.push_back(std::move(pass));
        }

        void AddResource(const std::string& name, RenderTarget* target, bool external = false) {
            resources[name] = { target, name, external };
        }

        void Compile();

        std::vector<VkFramebuffer> CreateFramebufferForTarget(Device& device, const RenderTarget& target, VkRenderPass renderPass);
        void Build(Device& device, const std::unordered_map<PassType, VkRenderPass>& passLayouts);

        void Execute(VkCommandBuffer cmd, RenderData data, uint32_t currentFrame);
    
    
    };
}