#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"
#include "Internal/Tools.h"
#include "Component/Tools/Material.h"


namespace Simple3D {


    /*
    needs textures (handeled by Materials)
    Material texture bindings on GPU handeled by model
    CreateModel(FILENAME) - creates a model bound with textures and all returns a pointer to said model -- DOES NOT MANAGE MEMORY
    */


    class Model {
    public:
        Model(std::vector<Vertex> verticies, std::vector<uint32_t> indices);
        ~Model();

        // Needed to be used by renderer but dont fuck w it if ur not renderer
        VkBuffer GetVertexBuffer();
        VkBuffer GetIndexBuffer();

        std::vector<Vertex> GetVerticies();
        bool hasBuffer();

        void CreateBuffers(Device* device, VkCommandPool* commandPool);
        void CreateBuffers();
        void DestroyBuffers();


        void BindMaterial(Material* newMaterial);

        void SetTransform(glm::mat4 transform);
        glm::mat4 GetTransform();

        std::vector<Vertex> Verticies;
        std::vector<uint32_t> Indices;


        // Set for shaders and textures
        Material* material;
    private:
        // Vulkan info
        Device* r_device = nullptr;
        VkCommandPool* r_commandPool;

        // Buffers
        VkBuffer vertexBuffer;
        VkBuffer indexBuffer;

        // Buffer Memory
        VkDeviceMemory indexBufferMemory;
        VkDeviceMemory vertexBufferMemory;

        // Textures
        /*
        Map with name of texture and the corrosponding texture info
        */
        TextureBinding albeado;



        // transform
        glm::mat4 transform;

        bool BufferEnabled = false;

        void CreateVertexBuffer();
        void CreateIndexBuffer();
    };
} 