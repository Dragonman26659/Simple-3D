#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"
#include "Internal/Tools.h"

namespace Simple3D {




    class Model {
    public:
        Model(std::vector<Vertex> verticies, std::vector<uint16_t> indices);
        ~Model();

        // Needed to be used by renderer but dont fuck w it if ur not renderer
        VkBuffer GetVertexBuffer();
        VkBuffer GetIndexBuffer();

        std::vector<Vertex> GetVerticies();
        bool hasBuffer();

        void CreateBuffers(Device* device, VkCommandPool* commandPool);
        void DestroyBuffers();

        void SetTransform(glm::mat4 transform);
        glm::mat4 GetTransform();

        std::vector<Vertex> Verticies;
        std::vector<uint16_t> Indices;
    private:
        // Vulkan info
        Device* r_device = nullptr;
        VkCommandPool* r_commandPool;

        // Buffers
        VkBuffer vertexBuffer;
        VkBuffer indexBuffer;

        // Buffer Memory
        VkDeviceMemory indexBufferMemory;
        VkDeviceMemory vertexBufferMemory;

        // transform
        glm::mat4 transform;

        bool BufferEnabled = false;

        void CreateVertexBuffer();
        void CreateIndexBuffer();
    };
}