#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"
#include "Internal/Tools.h"

namespace Simple3D {
    class Model {
    public:
        Model(std::vector<Vertex> verticies);


        // Needed to be used by renderer but dont fuck w it if ur not renderer
        VkBuffer GetVertexBuffer();
        std::vector<Vertex> GetVerticies();
        void DestroyVkBuffer();
        void CreateVertexBuffer(Device* device);
        bool hasBuffer();


    private:
        std::vector<Vertex> Verticies;


        // Vulkan info
        Device* r_device = nullptr;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;

        bool BufferEnabled = false;
    };
}