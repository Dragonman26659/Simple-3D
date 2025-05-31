#pragma once
#include "SimpleCore.h"

#include "Device.h"
#include "Tools.h"

#include "Component/Tools/Material.h"

namespace Simple3D {

	// Configures ALL pipelines together
	struct PipelineConfig {

	};


	class Pipeline {
	public:
		Pipeline(Device& s_Device, VkRenderPass& renderPass, Material* materialBinding);
		~Pipeline();


		VkPipeline			GetPipeline();
		VkPipelineLayout	GetLayout();
		void				updateUniformBuffer(uint32_t currentImage, glm::mat4 PerspectiveMatrix, glm::mat4 ViewMatrix, glm::mat4 transform);


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




		void			CreatePipeline();
		VkShaderModule	createShaderModule(const std::vector<char>& code);



		// Uniform Buffers & samplers
		void			createUniformBuffers();
		void			createDescriptorSetLayout();
		void			createDescriptorPool();
		void			createDescriptorSets();
	};
}