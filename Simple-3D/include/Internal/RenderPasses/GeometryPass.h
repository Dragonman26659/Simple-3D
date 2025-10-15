#pragma once
#include "Internal/RenderPass.h"




namespace Simple3D {
    class GeometryPass : public RenderPass {
    public:
        Device* device;
        Camera* camera;

        GeometryPass(Device* device, Camera* camera);
        void Execute(VkCommandBuffer cmd, RenderData data, uint32_t currentFrame) override;
    };
}