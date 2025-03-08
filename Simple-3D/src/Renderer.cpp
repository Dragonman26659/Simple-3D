#include "Renderer.h"

#ifdef DEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Helper function for error messages
std::string vkResultToString(VkResult result) {
	switch (result) {
	case VK_SUCCESS: return "VK_SUCCESS";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
	default: return "Unknown error";
	}
}

namespace Simple3D {

	// Initalisation
	Renderer::~Renderer() {
		vkDestroyInstance(instance, nullptr);
		setupDebugMessenger();
	}

	void Renderer::CreateInstance(std::string EngineName, std::string ApplicationName) {
		// Validation layers -- Checking for supprot
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			printf("validation layers requested, but not available!");
			throw std::runtime_error("validation layers requested, but not available!");
		}


		// App inforamtion
		VkApplicationInfo appInfo{}; 
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; 
		appInfo.pApplicationName = ApplicationName.c_str(); 
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); 
		appInfo.pEngineName = EngineName.c_str();; 
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); 
		appInfo.apiVersion = VK_API_VERSION_1_0; 



		// At minimum have the window extensions
		//uint32_t extensionCount = WindowExtensionCount;

		// Create the instance

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount = 0;

		auto extensions = getRequiredExtensions(); 
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data(); 

		// Create validation layers
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		// Create instance with error handling
		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
		if (result != VK_SUCCESS) {
			printf("Error: Failed to create Vulkan instance (error code: %s)\n", (vkResultToString(result).c_str()));
			throw std::runtime_error("failed to create instance: " +
				std::string(vkResultToString(result)));
		}
	}

	bool Renderer::checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}


	std::vector<const char*> Renderer::getRequiredExtensions() {

		std::vector<const char*> extensions(WindowExtensions, WindowExtensions + WindowExtensionCount);

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}



	//void Renderer::setupDebugMessenger() {
	//	if (!enableValidationLayers) return;
	//
	//	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	//	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	//	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	//	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	//	createInfo.pfnUserCallback = debugCallback;
	//	createInfo.pUserData = nullptr;
	//
	//}



	void Renderer::Render() {

	}




}