#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"

namespace Simple3D {
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, Device* device);

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, Device* device);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, Device* device, VkCommandPool* commandPool);
}