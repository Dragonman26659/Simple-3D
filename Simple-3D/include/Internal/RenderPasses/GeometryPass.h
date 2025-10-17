#pragma once
#include "Internal/RenderPass.h"




namespace Simple3D {
    class GeometryPass : public RenderPass {
    public:
        Camera* camera;

        GeometryPass(Camera* camera);
        void Execute(VkCommandBuffer cmd, const RenderData& data, uint32_t currentFrame) override;
    };
}