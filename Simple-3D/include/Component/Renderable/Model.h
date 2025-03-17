#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"

// Test for now (will replace with model class later on)
const std::vector<Vertex> TriangleVertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

namespace Simple3D {
    class Model {
    public:
        Model(std::vector<Vertex> verticies);




        // Needed to be used by renderer but dont fuck w it if ur not renderer
        VkBuffer GetVertexBuffer();
        void DestroyVkBuffer();
        void CreateVertexBuffer(Device& device);
        bool hasBuffer();


    private:
        std::vector<Vertex> Verticies;


        // Vulkan info
        Device& r_device;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;

        bool BufferEnabled = false;

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    };
}