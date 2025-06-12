#include "Component/Renderable/Model.h"


namespace Simple3D {

    Model::Model(std::vector<Vertex> verticies, std::vector<uint32_t> indices)
        : Verticies(verticies), Indices(indices) {

    }

    Model::~Model() {
        if (BufferEnabled) {
            DestroyBuffers();
        }
    }

    void Model::CreateBuffers(Device* device, VkCommandPool* commandPool) {
        r_device = device;
        r_commandPool = commandPool;


        CreateVertexBuffer();
        CreateIndexBuffer();

        BufferEnabled = true;
    }

    void Model::CreateBuffers() {
        CreateVertexBuffer();
        CreateIndexBuffer();

        BufferEnabled = true;
    }

    VkBuffer Model::GetVertexBuffer() {
        return vertexBuffer; 
    }

    VkBuffer Model::GetIndexBuffer() {
        return indexBuffer;
    }

    void Model::DestroyBuffers() {
        vkDestroyBuffer(r_device->getLogicalDevice(), vertexBuffer, nullptr);
        vkFreeMemory(r_device->getLogicalDevice(), vertexBufferMemory, nullptr);

        vkDestroyBuffer(r_device->getLogicalDevice(), vertexBuffer, nullptr);
        vkFreeMemory(r_device->getLogicalDevice(), vertexBufferMemory, nullptr);





        // Make sure we know buffer is gone and dont keep track of device or command pool anymore
        r_device = nullptr;
        r_commandPool = nullptr;
        BufferEnabled = false;
    }

    void Model::CreateVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(Verticies[0]) * Verticies.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, r_device);

        void* data;
        vkMapMemory(r_device->getLogicalDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, Verticies.data(), (size_t)bufferSize);
        vkUnmapMemory(r_device->getLogicalDevice(), stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory, r_device);

        copyBuffer(stagingBuffer, vertexBuffer, bufferSize, r_device, r_commandPool);

        vkDestroyBuffer(r_device->getLogicalDevice(), stagingBuffer, nullptr);
        vkFreeMemory(r_device->getLogicalDevice(), stagingBufferMemory, nullptr);
    }

    void Model::CreateIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(Indices[0]) * Indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, r_device);

        void* data;
        vkMapMemory(r_device->getLogicalDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, Indices.data(), (size_t)bufferSize);
        vkUnmapMemory(r_device->getLogicalDevice(), stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory, r_device);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize, r_device, r_commandPool);

        vkDestroyBuffer(r_device->getLogicalDevice(), stagingBuffer, nullptr);
        vkFreeMemory(r_device->getLogicalDevice(), stagingBufferMemory, nullptr);
    }

    void Model::BindMaterial(Material* newMaterial) {
        material = newMaterial;
    }


    void Model::SetTransform(const glm::mat4 n_transform) {
          transform = n_transform;
    }


    bool Model::hasBuffer() {
        return BufferEnabled;
    }

    std::vector<Vertex> Model::GetVerticies() {
        return Verticies;
    }

    const glm::mat4 Model::GetTransform() {
        return transform;
    }
} 