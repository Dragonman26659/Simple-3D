#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"
#include "Internal/Tools.h"
#include "Internal/Material/ShaderSet.h"

namespace Simple3D {
	// Needed when creating material by renderer
	struct MaterialInfo {

		ShaderSet* shaders;
		std::unordered_map<std::string, std::string> textures;
	};


	// Holds Tetxures and shaders, can be bound to models

	/*
	* Takes a vector of textures and a vertex source file and a fraagment source file
	*/
	class Material {
	public:
		Material(Device* device, VkCommandPool* commandPool, std::unordered_map<std::string, std::string> textures, ShaderSet* shaders);
		~Material();


		void updateSortedTextureNames();
		int getTextureIndex(const std::string& name) const;

		Device* r_device = nullptr;
		VkCommandPool* r_commandPool;
		std::vector<VkDescriptorSet> descriptorSets;

		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

		void CreateDescriptorSets();
		void UpdateDescriptors(uint32_t frameIndex);

		ShaderSet* shaders = nullptr;
		bool CreatedDescriptors = false;
		bool DescritorsNeeded = true;

		std::unordered_map<std::string, TextureBinding> textures;
		std::vector<std::string> sortedTextureNames;
	private:

		// texture Bindings
		TextureBinding CreateTexture(std::string texture);
		void DestroyImage(TextureBinding binding);
	};
}