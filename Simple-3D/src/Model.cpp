#include "Component/Renderable/Model.h"
#include "Internal/Allocator.h"   // VMA helper
#include "Internal/Tools.h"       // createBuffer (VMA overload), copyBuffer

namespace Simple3D {

    Model::Model(std::vector<Vertex>   verticies,
        std::vector<uint32_t> indices,
        bool                  isDynamic)
        : Verticies(verticies), Indices(indices), DynamicModel(isDynamic)
    {
    }

    Model::~Model()
    {
        if (BufferEnabled)
            DestroyBuffers();
    }

    // ── CreateBuffers ─────────────────────────────────────────────────────────────
    void Model::CreateBuffers(Device* device, VkCommandPool* commandPool)
    {
        r_device = device;
        r_commandPool = commandPool;
        CreateVertexBuffer();
        CreateIndexBuffer();
        BufferEnabled = true;
    }

    void Model::CreateBuffers()
    {
        CreateVertexBuffer();
        CreateIndexBuffer();
        BufferEnabled = true;
    }

    // ── Getters ───────────────────────────────────────────────────────────────────
    VkBuffer Model::GetVertexBuffer() { return vertexBuffer; }
    VkBuffer Model::GetIndexBuffer() { return indexBuffer; }

    // ── DestroyBuffers ────────────────────────────────────────────────────────────
    void Model::DestroyBuffers()
    {
        // Unmap before destroying
        if (DynamicModel && vertexBufferMapped)
        {
            vmaUnmapMemory(Allocator::Get(), vertexAlloc.handle);
            vertexBufferMapped = nullptr;
        }

        if (vertexBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(Allocator::Get(), vertexBuffer, vertexAlloc.handle);
            vertexBuffer = VK_NULL_HANDLE;
            vertexAlloc = {};
        }

        if (indexBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(Allocator::Get(), indexBuffer, indexAlloc.handle);
            indexBuffer = VK_NULL_HANDLE;
            indexAlloc = {};
        }

        r_device = nullptr;
        r_commandPool = nullptr;
        BufferEnabled = false;
    }

    // ── CreateVertexBuffer ────────────────────────────────────────────────────────
    void Model::CreateVertexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(Verticies[0]) * Verticies.size();

        // Build usage flags.  RT requires SHADER_DEVICE_ADDRESS_BIT and
        // ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR on vertex buffers.
        // We always include both so that BLAS builds work without re-creating buffers.
        const VkBufferUsageFlags rtFlags =
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

        if (DynamicModel)
        {
            // --- Dynamic: host-visible + coherent, persistently mapped ---
            createBuffer(bufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rtFlags,
                VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT,
                vertexBuffer, vertexAlloc,
                r_device);

            // VMA fills alloc.info.pMappedData for MAPPED allocations.
            vertexBufferMapped = vertexAlloc.info.pMappedData;
            memcpy(vertexBufferMapped, Verticies.data(), static_cast<size_t>(bufferSize));
        }
        else
        {
            // --- Static: staging → device-local ---
            VkBuffer   stagingBuffer{};
            Allocation stagingAlloc{};
            createBuffer(bufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                stagingBuffer, stagingAlloc,
                r_device);

            void* mapped{};
            vmaMapMemory(Allocator::Get(), stagingAlloc.handle, &mapped);
            memcpy(mapped, Verticies.data(), static_cast<size_t>(bufferSize));
            vmaUnmapMemory(Allocator::Get(), stagingAlloc.handle);

            createBuffer(bufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rtFlags,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                0,
                vertexBuffer, vertexAlloc,
                r_device);

            copyBuffer(stagingBuffer, vertexBuffer, bufferSize, r_device, r_commandPool);

            vmaDestroyBuffer(Allocator::Get(), stagingBuffer, stagingAlloc.handle);
        }
    }

    // ── CreateIndexBuffer ─────────────────────────────────────────────────────────
    void Model::CreateIndexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(Indices[0]) * Indices.size();

        const VkBufferUsageFlags rtFlags =
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

        VkBuffer   stagingBuffer{};
        Allocation stagingAlloc{};
        createBuffer(bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            stagingBuffer, stagingAlloc,
            r_device);

        void* mapped{};
        vmaMapMemory(Allocator::Get(), stagingAlloc.handle, &mapped);
        memcpy(mapped, Indices.data(), static_cast<size_t>(bufferSize));
        vmaUnmapMemory(Allocator::Get(), stagingAlloc.handle);

        createBuffer(bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rtFlags,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            0,
            indexBuffer, indexAlloc,
            r_device);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize, r_device, r_commandPool);
        vmaDestroyBuffer(Allocator::Get(), stagingBuffer, stagingAlloc.handle);
    }

    // ── Misc ──────────────────────────────────────────────────────────────────────
    void Model::BindMaterial(Material* newMaterial) { material = newMaterial; }

    void Model::SetTransform(const glm::mat4 n_transform) { transform = n_transform; }

    bool Model::hasBuffer() { return BufferEnabled; }

    std::vector<Vertex> Model::GetVerticies() { return Verticies; }

    const glm::mat4 Model::GetTransform() { return transform; }

    // ── UpdateVerticies ───────────────────────────────────────────────────────────
    bool Model::UpdateVerticies(std::vector<Vertex> newVerticies)
    {
        if (!BufferEnabled || vertexBuffer == VK_NULL_HANDLE || !DynamicModel)
            return false;

        if (newVerticies.size() != Verticies.size())
            return false;

        Verticies = std::move(newVerticies);
        VkDeviceSize bufferSize = sizeof(Verticies[0]) * Verticies.size();

        if (vertexBufferMapped)
        {
            // Fast path: VMA_ALLOCATION_CREATE_MAPPED_BIT keeps it permanently mapped.
            memcpy(vertexBufferMapped, Verticies.data(), static_cast<size_t>(bufferSize));
        }
        else
        {
            void* mapped{};
            vmaMapMemory(Allocator::Get(), vertexAlloc.handle, &mapped);
            memcpy(mapped, Verticies.data(), static_cast<size_t>(bufferSize));
            vmaUnmapMemory(Allocator::Get(), vertexAlloc.handle);
        }

        return true;
    }

} // namespace Simple3D