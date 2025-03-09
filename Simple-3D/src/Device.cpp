#include "Internal/Device.h"



namespace Simple3D {
	Device::Device(VkInstance& instance, VkSurfaceKHR& surface) : instance(instance), surface(surface){
		PickPhysicalDevice();
		createLogicalDevice();
	}



	Device::~Device() {
		vkDestroyDevice(device, nullptr); 
	}


	// Physical Device
	void Device::PickPhysicalDevice() {
		physicalDevice = VK_NULL_HANDLE;


		uint32_t deviceCount = 0;  
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr); 


		if (deviceCount == 0) { 
			printf("failed to find GPUs with Vulkan support!"); 
			throw std::runtime_error("failed to find GPUs with Vulkan support!"); 
		}

		std::vector<VkPhysicalDevice> devices(deviceCount); 
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()); 


		// should be replaced with a better evaluation of devices but this works for now
		for (const auto& device : devices) { 
			if (isDeviceSuitable(device)) { 
				physicalDevice = device; 
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE) { 
			printf("failed to find a suitable GPU!"); 
			throw std::runtime_error("failed to find a suitable GPU!"); 
		}
	}



	// Determines if we can use a device or not
	bool Device::isDeviceSuitable(VkPhysicalDevice p_device) {
		QueueFamilyIndices indices = findQueueFamilies(p_device);

		bool extensionsSupported = checkDeviceExtensionSupport(p_device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(p_device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;

	}

	bool Device::checkDeviceExtensionSupport(VkPhysicalDevice p_device) {
		uint32_t extensionCount; 
		vkEnumerateDeviceExtensionProperties(p_device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(p_device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}


	SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice p_device) {
		SwapChainSupportDetails details; 

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_device, surface, &details.capabilities);


		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &formatCount, details.formats.data());
		}


		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &presentModeCount, details.presentModes.data());
		}



		return details;
	}

	SwapChainSupportDetails Device::querySwapChainSupport() {
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);


		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
		}


		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
		}



		return details;
	}


	QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice p_device) {
		QueueFamilyIndices indices{};

		// Validate the device pointer
		if (p_device == VK_NULL_HANDLE) {
			return indices;
		}


		// Get list of queue families
		uint32_t queueFamilyCount = 0; 
		vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount); 
		vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueFamilyCount, queueFamilies.data());

		// Find suitable queue family
		int i = 0;
		for (const auto& queueFamily : queueFamilies) { 
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface, &presentSupport);

			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) { 
				indices.graphicsFamily = i; 
			}

			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.isComplete()) {
				break;
			}

			i++;
		}

		return indices;
	}

	QueueFamilyIndices Device::findQueueFamilies() {
		QueueFamilyIndices indices{};

		// Validate the device pointer
		if (physicalDevice == VK_NULL_HANDLE) {
			return indices;
		}


		// Get list of queue families
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

		// Find suitable queue family
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			VkBool32 presentSupport = false; 
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.isComplete()) {
				break;
			}

			i++;
		}

		return indices;
	}

	void Device::createLogicalDevice() {
		// Specify queues to be created
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;

		float queuePriority = 1.0f; 
		queueCreateInfo.pQueuePriorities = &queuePriority; 



		// probably will need to change this later
		VkPhysicalDeviceFeatures deviceFeatures{};
		// Currently this struct is left blank



		// Create the logical device
		VkDeviceCreateInfo createInfo{}; 
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; 

		createInfo.pQueueCreateInfos = &queueCreateInfo; 
		createInfo.queueCreateInfoCount = 1; 

		createInfo.pEnabledFeatures = &deviceFeatures; 

		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data(); 


		// currenyly validation layers not working :(
		//if (enableValidationLayers) {
		//	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		//	createInfo.ppEnabledLayerNames = validationLayers.data();
		//}
		//else {
		//	createInfo.enabledLayerCount = 0;
		//}


		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) { 
			printf("failed to create logical device!");
			throw std::runtime_error("failed to create logical device!"); 
		}

		// TEMP - probably
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue); 
	}

	VkDevice Device::getLogicalDevice() {
		return device;
	}
}