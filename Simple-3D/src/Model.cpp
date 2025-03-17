#include "Component/Renderable/Model.h"



namespace Simple3D {
    Model::Model(std::vector<Vertex> verticies)
        : Verticies(verticies) {

    }


    VkBuffer Model::GetVertexBuffer() {
        return vertexBuffer;
    }

    void Model::DestroyVkBuffer() {
        vkDestroyBuffer(r_device.getLogicalDevice(), vertexBuffer, nullptr);
        vkFreeMemory(r_device.getLogicalDevice(), vertexBufferMemory, nullptr);

        // Make sure we know buffer is gone
        BufferEnabled = false;
    }

    void Model::CreateVertexBuffer(Device& device) {
        r_device = device;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(Verticies[0]) * Verticies.size();
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device.getLogicalDevice(), &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vertex buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device.getLogicalDevice(), vertexBuffer, &memRequirements);


        VkMemoryAllocateInfo allocInfo{}; 
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; 
        allocInfo.allocationSize = memRequirements.size; 
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); 


        if (vkAllocateMemory(device.getLogicalDevice(), &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        vkBindBufferMemory(device.getLogicalDevice(), vertexBuffer, vertexBufferMemory, 0);

        // Make sure we know it has a buffer
        BufferEnabled = true;
    }

    bool Model::hasBuffer() {
        return BufferEnabled;
    }

    uint32_t Model::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties; 
        vkGetPhysicalDeviceMemoryProperties(r_device.getPhysicalDevice(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }
} 