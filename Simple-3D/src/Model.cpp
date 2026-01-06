#include "Component/Renderable/Model.h"


namespace Simple3D {

    Model::Model(std::vector<Vertex> verticies, std::vector<uint32_t> indices, bool isDynamic)
        : Verticies(verticies), Indices(indices), DynamicModel(isDynamic) {

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
        if (DynamicModel && vertexBufferMapped) {
            vkUnmapMemory(r_device->getLogicalDevice(), vertexBufferMemory);
            vertexBufferMapped = nullptr;
        }

        if (vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(r_device->getLogicalDevice(), vertexBuffer, nullptr);
            vertexBuffer = VK_NULL_HANDLE;
        }

        if (vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(r_device->getLogicalDevice(), vertexBufferMemory, nullptr);
            vertexBufferMemory = VK_NULL_HANDLE;
        }

        r_device = nullptr;
        r_commandPool = nullptr;
        BufferEnabled = false;
    }

    void Model::CreateVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(Verticies[0]) * Verticies.size();

        if (DynamicModel) {
            // --- Dynamic mesh: host-visible, coherent, persistently mapped ---
            createBuffer(bufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertexBuffer,
                vertexBufferMemory,
                r_device);

            // Optionally persistently map (for max performance)
            vkMapMemory(r_device->getLogicalDevice(), vertexBufferMemory, 0, bufferSize, 0, &vertexBufferMapped);
            memcpy(vertexBufferMapped, Verticies.data(), (size_t)bufferSize);

        }
        else {
            // --- Static mesh: staging buffer → device-local ---
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(bufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer,
                stagingBufferMemory,
                r_device);

            // Copy vertex data to staging buffer
            void* data;
            vkMapMemory(r_device->getLogicalDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, Verticies.data(), (size_t)bufferSize);
            vkUnmapMemory(r_device->getLogicalDevice(), stagingBufferMemory);

            // Create device-local vertex buffer
            createBuffer(bufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                vertexBuffer,
                vertexBufferMemory,
                r_device);

            copyBuffer(stagingBuffer, vertexBuffer, bufferSize, r_device, r_commandPool);

            // Clean up staging
            vkDestroyBuffer(r_device->getLogicalDevice(), stagingBuffer, nullptr);
            vkFreeMemory(r_device->getLogicalDevice(), stagingBufferMemory, nullptr);
        }
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

    bool Model::UpdateVerticies(std::vector<Vertex> newVerticies)
    {
        if (!BufferEnabled || vertexBuffer == VK_NULL_HANDLE || !DynamicModel)
            return false;

        if (newVerticies.size() != Verticies.size())
            return false;

        Verticies = std::move(newVerticies);

        VkDeviceSize bufferSize = sizeof(Verticies[0]) * Verticies.size();

        if (vertexBufferMapped) {
            // --- Fast path: persistently mapped ---
            memcpy(vertexBufferMapped, Verticies.data(), (size_t)bufferSize);
        }
        else {
            // --- Fallback: map/unmap each frame ---
            void* data;
            vkMapMemory(r_device->getLogicalDevice(), vertexBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, Verticies.data(), (size_t)bufferSize);
            vkUnmapMemory(r_device->getLogicalDevice(), vertexBufferMemory);
        }

        return true;
    }
}