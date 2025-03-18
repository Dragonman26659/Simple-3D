#include "Component/Renderable/Model.h"



namespace Simple3D {
    Model::Model(std::vector<Vertex> verticies)
        : Verticies(verticies) {

    }


    VkBuffer Model::GetVertexBuffer() {
        return vertexBuffer;
    }

    void Model::DestroyVkBuffer() {
        vkDestroyBuffer(r_device->getLogicalDevice(), vertexBuffer, nullptr);
        vkFreeMemory(r_device->getLogicalDevice(), vertexBufferMemory, nullptr);


        // Make sure we know buffer is gone and dont keep track of device anymore
        r_device = nullptr;
        BufferEnabled = false;
    }

    void Model::CreateVertexBuffer(Device* device) {
        r_device = device;


        VkDeviceSize bufferSize = sizeof(Verticies[0]) * Verticies.size();
        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer, vertexBufferMemory, *r_device);

        void* data;
        vkMapMemory(r_device->getLogicalDevice(), vertexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, Verticies.data(), (size_t)bufferSize);
        vkUnmapMemory(r_device->getLogicalDevice(), vertexBufferMemory);

        BufferEnabled = true;
    }

    bool Model::hasBuffer() {
        return BufferEnabled;
    }

    std::vector<Vertex> Model::GetVerticies() {
        return Verticies;
    }
} 