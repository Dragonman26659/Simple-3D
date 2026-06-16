#pragma once
#include "SimpleCore.h"


struct QueueFamilyIndices { 
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

namespace Simple3D {
	/*
	* Handles physical and logical vulkan devices
	*/
	class Device {
	public:
		Device(VkInstance& instance, VkSurfaceKHR& surface);
		~Device();

		SwapChainSupportDetails querySwapChainSupport();
		QueueFamilyIndices findQueueFamilies();
		VkDevice getLogicalDevice();
		VkPhysicalDevice getPhysicalDevice();
		VkQueue getVKgraphicsQueue();
		VkQueue getVKpresentQueue();
		VkPhysicalDeviceProperties GetProperties();
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

	private:
		VkInstance& instance;
		VkSurfaceKHR& surface;


		VkPhysicalDevice physicalDevice;
		VkDevice device;

		VkQueue graphicsQueue;
		VkQueue presentQueue;


		// Physical Device
		void PickPhysicalDevice();
		bool isDeviceSuitable(VkPhysicalDevice p_device);
		bool checkDeviceExtensionSupport(VkPhysicalDevice p_device);

		// Swap chain
		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

		// logical Device
		void createLogicalDevice();

		// Queue Families
		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	};
}