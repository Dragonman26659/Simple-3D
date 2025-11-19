#pragma once
#include "SimpleCore.h"
#include "RenderPass.h"



namespace Simple3D {
    class RenderGraph {
    public:
        RenderGraph(std::string name, VkCommandPool& commandPool, Device& device);
        ~RenderGraph();

        std::string GetName() { return name; }

        void AddPass(RenderPass* pass) {
            passes.push_back(std::move(pass));
        }

        void AddResource(const std::string& name, RenderTarget* target, bool external = false) {
            resources[name] = { target, name, external };
        }

        void Compile();

        std::vector<VkFramebuffer> CreateFramebufferForTarget(Device& device, RenderTarget& target, VkRenderPass renderPass);
        void Build(Device& device, std::vector<ShaderSet*> shaders);

        void Execute(VkCommandBuffer cmd, RenderData data, uint32_t currentFrame);
        void Generate(Device& device, std::vector<ShaderSet*> shaders);

        void RegenerateFramebuffers(Device& device);
    
    private:
        VkCommandPool& commandPool;

        std::string name;


        std::unordered_map<std::string, RenderResource> resources; // Not memmanaged by this
        std::vector<RenderPass*> passes; // not mem manageed by this
        std::vector<RenderPass*> executionOrder;  // not mem manageed by this

        // store logical device for cleanup
        VkDevice logicalDevice = VK_NULL_HANDLE;


    };
}