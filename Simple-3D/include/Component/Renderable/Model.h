#pragma once
#include "SimpleCore.h"
#include "Internal/Device.h"
#include "Internal/Tools.h"
#include "Component/Tools/Material.h"
#include "Internal/Allocator.h"


namespace Simple3D {

    // ── Model ─────────────────────────────────────────────────────────────────────
    // Owns a vertex buffer and an index buffer.  Both are created through VMA.
    //
    // RT note: vertex and index buffers always include
    //   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    //   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
    // so that BLAS builds never require a buffer re-create.
    //
    class Model {
    public:
        Model(std::vector<Vertex> verticies, std::vector<uint32_t> indices, bool isDynamic = false);
        ~Model();

        // Create GPU buffers.  The two-argument overload stores device + pool for
        // later use (e.g. dynamic updates, buffer re-creation).
        void CreateBuffers(Device* device, VkCommandPool* commandPool);
        void CreateBuffers();   // assumes r_device / r_commandPool already set

        void DestroyBuffers();

        VkBuffer GetVertexBuffer();
        VkBuffer GetIndexBuffer();

        void BindMaterial(Material* newMaterial);
        void SetTransform(const glm::mat4 n_transform);

        bool hasBuffer();
        std::vector<Vertex> GetVerticies();
        const glm::mat4     GetTransform();

        // Update vertex data in-place (dynamic models only, same vertex count).
        bool UpdateVerticies(std::vector<Vertex> newVerticies);

        // Public data accessed by render passes and RT builders.
        std::vector<Vertex>   Verticies;
        std::vector<uint32_t> Indices;
        Material* material = nullptr;
        glm::mat4  transform = glm::mat4(1.0f);

        // Set by the engine-level renderer each frame.
        glm::mat4  Transform = glm::mat4(1.0f);
        bool       hasRendered = false;

    private:
        // GPU resources
        VkBuffer   vertexBuffer = VK_NULL_HANDLE;
        Allocation vertexAlloc = {};
        void* vertexBufferMapped = nullptr;  // non-null for dynamic models (MAPPED flag)

        VkBuffer   indexBuffer = VK_NULL_HANDLE;
        Allocation indexAlloc = {};

        // Context
        Device* r_device = nullptr;
        VkCommandPool* r_commandPool = nullptr;

        bool DynamicModel = false;
        bool BufferEnabled = false;

        void CreateVertexBuffer();
        void CreateIndexBuffer();
    };

} // namespace Simple3D