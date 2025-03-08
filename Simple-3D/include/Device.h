#pragma once
#include "vulkan/vulkan.h"

namespace Simple3D {
	/*
	* Handles physical and logical vulkan devices
	*/
	class Device {
	public:


	private:
		VkInstance& instance;



		// Physical Device
		void PickDevice();
		bool isDeviceSuitable(VkPhysicalDevice device);
	};
}