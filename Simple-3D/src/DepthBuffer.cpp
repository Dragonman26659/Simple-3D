#include "Internal/DepthBuffer.h"



namespace Simple3D {
    DepthBuffer::DepthBuffer(Device* device, VkExtent2D extent, VkCommandPool* commandPool, uint32_t layerCount)
        : device(device), extent(extent), layerCount(layerCount) {

        depthFormat = findDepthFormat(device);

        // 1. Create the Image with Layer Support
        // Note: Your 'createImage' helper needs to be updated to accept arrayLayers.
        // If it doesn't, you'll need to call vkCreateImage manually or update the helper.
        createImage(
            extent.width,
            extent.height,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depthImage,
            depthImageMemory,
            device,
            layerCount // Pass layerCount here
        );

        // 2. Create the Image View (2D or 2D_ARRAY)
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        // If layers > 1, it must be a 2D_ARRAY for shadow mapping
        viewInfo.viewType = (layerCount > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = layerCount; // Map all layers

        if (vkCreateImageView(device->getLogicalDevice(), &viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create depth image view!");
        }

        // 3. Transition the layout for all layers
        // Note: Your 'transitionImageLayout' helper must be updated to accept layerCount.
        transitionImageLayout(
            depthImage,
            depthFormat,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            device,
            commandPool,
            layerCount // Ensure the barrier covers the whole array
        );
    }

    DepthBuffer::~DepthBuffer() {
        vkDestroyImageView(device->getLogicalDevice(), depthImageView, nullptr);
        vkDestroyImage(device->getLogicalDevice(), depthImage, nullptr);
        vkFreeMemory(device->getLogicalDevice(), depthImageMemory, nullptr);
    }
}