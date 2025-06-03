#pragma once
#include "SimpleCore.h"

// Internal
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Internal/Pipeline.h"
#include "Internal/DepthBuffer.h"
#include "Internal/RenderTexture.h"


// Components
#include "Component/Renderable/Model.h"
#include "Component/Tools/Camera.h"
#include "Component/Tools/Lights.h"


namespace Simple3D {
    class RenderInstance {
    private:
        // Vulkan info
        VkRenderPass renderPass;
        RenderTexture texture;

        // Render info
        Camera* camera;
        std::vector<Model*> models;
        std::vector<Light> lights;

        Device* RenderDevice;
        SwapChain* swapChain;

        // Store one pipeline per material
        std::unordered_map<Material*, Pipeline*> materials;


        bool RenderToTexture;

    public:
        RenderInstance(Device* RenderDevice, SwapChain* swapChain, VkRenderPass renderPass)
            : RenderDevice(RenderDevice), swapChain(swapChain), renderPass(renderPass), RenderToTexture(false) {
        }

        RenderInstance(Device* RenderDevice, SwapChain* swapChain, VkRenderPass renderPass, RenderTexture texture);

        ~RenderInstance();

        void SetCamera(Camera* cam) {
            camera = cam;
        }

        void SubmitModel(Model* model) {
            models.push_back(model);
        }

        void SubmitLight(Light& light) {
            lights.push_back(light);
        }

        void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, VkCommandPool commandPool, uint32_t currentFrame, VkFramebuffer framebuffer);


        void ClearVectors();

        void CreateMaterial(Material* material, VkCommandPool commandPool);
    };
}