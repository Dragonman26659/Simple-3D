#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"
#include "Internal/Tools.h"


namespace Simple3D {
	// Needed when creating material by renderer
	struct MaterialInfo {
		// Shader info
		std::string vertexSource;
		std::string FragmentSource;

		bool isLit;
		std::vector<std::string> textures;
	};


	// Holds Tetxures and shaders, can be bound to models

	/*
	* Takes a vector of textures and a vertex source file and a fraagment source file
	*/
	class Material {
	public:
		Material(Device* r_device, VkCommandPool* commandPool, std::string vertexSource, std::string FragmentSource, std::vector<std::string> textureNames);
		~Material();


		void updateSortedTextureNames();
		int getTextureIndex(const std::string& name) const;

		Device* r_device = nullptr;
		VkCommandPool* r_commandPool;


		// Shaders
		std::string vertexSource;
		std::string FragmentSource;

		// Do we need to light the texture
		bool isLit;


		// Textures
		std::unordered_map<std::string, TextureBinding> textures;
		// Sort texture names
		std::vector<std::string> sortedTextureNames;
	private:


		// texture Bindings
		TextureBinding CreateTexture(std::string texture);
		void createTextureSampler(TextureBinding* binding);
		void DestroyImage(TextureBinding binding);
	};
}