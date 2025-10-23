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

		int width, height;
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

	void createTextureSampler(TextureBinding* binding, Device* device);


	TextureBinding CreateTextureBinding(std::string filepath, Device* r_device, VkCommandPool* r_commandPool);

	static VkDeviceSize getAlignedSize(VkDeviceSize original, VkDeviceSize alignment) {
		if (alignment == 0) return original;
		return (original + alignment - 1) & ~(alignment - 1);
	}

	// Give any Vulkan object a debug name for RenderDoc / Nsight / validation layers
	inline void SetObjectName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const std::string& name)
	{
		if (name.empty()) return; // skip unnamed
		if (!device) return;

		VkDebugUtilsObjectNameInfoEXT nameInfo{};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = objectType;
		nameInfo.objectHandle = objectHandle;
		nameInfo.pObjectName = name.c_str();

		// Look up the function pointer (it’s an extension function)
		auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
		if (func)
			func(device, &nameInfo);
	}
}