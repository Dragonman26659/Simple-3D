#pragma once
#include "Internal/RenderPass.h"




namespace Simple3D {
    class ForwardPass : public RenderPass {
    public:
        Camera* camera;

        ForwardPass(Camera* camera);
        void Execute(VkCommandBuffer cmd, const RenderData& data, uint32_t currentFrame, uint32_t syncIndex) override;
        void SetUpPass() override;
    };
}