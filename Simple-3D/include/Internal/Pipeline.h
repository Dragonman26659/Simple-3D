#pragma once
#include "SimpleCore.h"

#include "Device.h"
#include "Tools.h"

#include "Component/Tools/Material.h"
#include "Component/Tools/Lights.h"

namespace Simple3D {

	// Configures ALL pipelines together
	struct PipelineConfig {

	};


	class Pipeline {
	public:
		Pipeline(Device& s_Device, VkRenderPass renderPass, Material* materialBinding);
		~Pipeline();


		VkPipeline			GetPipeline();
		VkPipelineLayout	GetLayout();
		void				updateUniformBuffer(uint32_t currentImage, uint32_t objectIndex, glm::mat4 PerspectiveMatrix, glm::mat4 ViewMatrix, glm::mat4 transform, glm::vec3 CameraPos);
		void				updateLights(uint32_t currentImage, const std::vector<Light>& lights);


		// ONly to be used when using Imgui
		void				setRenderPass(VkRenderPass newRenderPass);


		// Make discriptor sets public cuz fuck getters
		std::vector<VkDescriptorSet> descriptorSets;

	private:
		Device& s_Device;
		Material* material;

		static std::vector<char> readFile(const std::string& filename);

		// Render Pass Reference
		VkRenderPass& renderPass;

		// Pipeline itself
		VkPipelineLayout pipelineLayout;
		VkPipeline graphicsPipeline; 

		// Discriptor set
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorPool descriptorPool;

		// Uniform Buffers
		std::vector<VkBuffer> uniformBuffers;
		std::vector<VkDeviceMemory> uniformBuffersMemory;
		std::vector<void*> uniformBuffersMapped;

		// dynamic UBO stuff
		size_t dynamicAlignment = 0;
		VkDeviceSize dynamicBufferSize = 0;
		uint32_t maxObjects = 1024; // tweak to expected max objects per material
		std::vector<VkBuffer> dynamicUniformBuffers;             // per-frame big buffer
		std::vector<VkDeviceMemory> dynamicUniformBuffersMemory;
		std::vector<void*> dynamicUniformBuffersMapped;


		// Lights
		std::vector<VkBuffer> lightBuffers;
		std::vector<VkDeviceMemory> lightBuffersMemory;
		std::vector<void*> lightBuffersMapped;
		std::vector<VkDeviceSize> lightBuffersSize;


		std::vector<char> vertShaderCode;
		std::vector<char> fragShaderCode;



		void			CreatePipeline();
		VkShaderModule	createShaderModule(const std::vector<char>& code);



		// Uniform Buffers & samplers
		void			createUniformBuffers();
		void			createLightBuffers();


		void			createDescriptorSetLayout();
		void			createDescriptorPool();
		void			createDescriptorSets();
	};
}