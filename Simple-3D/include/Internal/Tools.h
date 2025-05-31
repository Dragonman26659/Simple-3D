#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"

namespace Simple3D {

	// Binding for each texture
	struct TextureBinding {
		VkImageView view;
		VkSampler sampler;
		VkDescriptorSet descriptorSet;

		// Image + image memory
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
	};



	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, Device* device);

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, Device* device);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, Device* device, VkCommandPool* commandPool);

	VkCommandBuffer beginSingleTimeCommands(Device* device, VkCommandPool* commandPool);

	void endSingleTimeCommands(Device* device, VkCommandPool* commandPool, VkCommandBuffer* commandBuffer);

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, Device* device, VkCommandPool* commandPool);

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, Device* device, VkCommandPool* commandPool);


	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, Device* device);

	VkFormat findDepthFormat(Device* device);

	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features, Device* device);

	bool hasStencilComponent(VkFormat format);

	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, Device* device);
}