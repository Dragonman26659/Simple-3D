#pragma once
#include "SimpleCore.h"
#include "Device.h"


namespace Simple3D {

	class SwapChain {
	public:
		SwapChain(Device& s_Device, VkSurfaceKHR& surface, int width, int height);
		~SwapChain();

		// Swap chain operations
		void create();
		void recreate(int n_width, int n_height);
		void cleanup();

		// Accessors
		const std::vector<VkImageView>& getImageViews() const;

	private:
		Device& s_Device;

		VkSurfaceKHR& surface;
		int width;
		int height;

		VkSwapchainKHR swapChain;
		std::vector<VkImage> swapChainImages;
		std::vector<VkImageView> swapChainImageViews; 
		VkFormat swapChainImageFormat;
		VkExtent2D swapChainExtent;


		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

		void createImageViews();
	};
}