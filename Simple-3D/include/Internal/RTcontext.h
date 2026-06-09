#pragma once
// RayTracingContext.h
// Detects RT hardware support at runtime and exposes the PFN table for
// VK_KHR_acceleration_structure + VK_KHR_ray_query.
//
// Usage:
//   RayTracingContext ctx;
//   ctx.Init(instance, physDevice, device);
//   if (ctx.IsAvailable()) { ... }
//
// The context is intentionally separate from Device so that applications
// that do not want RT pay zero overhead — just never call Init().

#include "SimpleCore.h"
#include "Internal/Allocator.h"

namespace Simple3D {

    // ── Feature-query result ─────────────────────────────────────────────────────
    struct RayTracingSupport {
        bool accelerationStructure = false; // VK_KHR_acceleration_structure
        bool rayQuery = false; // VK_KHR_ray_query (for frag-shader queries)
        bool rayTracingPipeline = false; // VK_KHR_ray_tracing_pipeline (full RT pipelines)
        bool bufferDeviceAddress = false; // required by AS
        bool descriptorIndexing = false; // required for bindless textures

        // True when at minimum the fragments-shader ray-query path works.
        bool SupportsRayQueryInFragmentShader() const {
            return accelerationStructure && rayQuery && bufferDeviceAddress;
        }
        // True when full RT pipelines (raygen/miss/hit shaders) are available.
        bool SupportsFullPipeline() const {
            return SupportsRayQueryInFragmentShader() && rayTracingPipeline;
        }
    };

    // ── PFN table for VK_KHR_acceleration_structure ──────────────────────────────
    struct AccelerationStructurePFN {
        PFN_vkCreateAccelerationStructureKHR        createAS = nullptr;
        PFN_vkDestroyAccelerationStructureKHR       destroyAS = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR getBuildSizes = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR     cmdBuildAS = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR getDeviceAddress = nullptr;
        PFN_vkCmdWriteAccelerationStructuresPropertiesKHR cmdWriteProps = nullptr;

        bool IsComplete() const {
            return createAS && destroyAS && getBuildSizes &&
                cmdBuildAS && getDeviceAddress;
        }
    };

    // ── RayTracingContext ─────────────────────────────────────────────────────────
    class RayTracingContext {
    public:
        RayTracingContext() = default;
        ~RayTracingContext() = default;

        // Call after Device and Allocator are initialised.
        // Queries extension support; if RT is present, loads the PFN table.
        void Init(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device);

        bool IsAvailable() const { return m_Support.SupportsRayQueryInFragmentShader(); }
        const RayTracingSupport& GetSupport() const { return m_Support; }
        const AccelerationStructurePFN& GetPFN()   const { return m_PFN; }

        VkDevice GetDevice() const { return m_Device; }

        // ── Acceleration structure helpers ────────────────────────────────────────

        // Creates a device-local buffer suitable for AS storage or scratch.
        // Sets VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT automatically.
        struct ASBuffer {
            VkBuffer      buffer = VK_NULL_HANDLE;
            Allocation    alloc = {};
            VkDeviceAddress address = 0;
        };

        ASBuffer CreateASBuffer(VkDeviceSize size,
            VkBufferUsageFlags extraUsage = 0);

        void DestroyASBuffer(ASBuffer& buf);

        // Returns the device address of an existing VkBuffer.
        VkDeviceAddress GetBufferAddress(VkBuffer buffer) const;

    private:
        VkDevice         m_Device = VK_NULL_HANDLE;
        RayTracingSupport m_Support = {};
        AccelerationStructurePFN m_PFN = {};

        void LoadPFNTable();

        // Checks whether a physical device supports all required extensions.
        static bool CheckExtensionSupport(VkPhysicalDevice physDevice);
    };

    // ── BLAS (Bottom-Level Acceleration Structure) ────────────────────────────────
    // One BLAS per static mesh.  Created by Model::BuildBLAS().
    // Holds only the AS handle and its backing buffer — all build scratch
    // memory is freed after construction.
    struct BLAS {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        RayTracingContext::ASBuffer backing = {};

        bool IsValid() const { return handle != VK_NULL_HANDLE; }
    };

    // ── TLAS (Top-Level Acceleration Structure) ───────────────────────────────────
    // One TLAS per rendered scene / render view, rebuilt each frame (or dirty-flagged).
    // Engine code owns TLAS; Simple3D provides BuildTLAS helpers.
    struct TLASInstance {
        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
        glm::mat4                  transform;  // row-major 3x4 stored as mat4
        uint32_t                   instanceID = 0;
        uint32_t                   hitGroupIndex = 0;
        uint8_t                    mask = 0xFF;
    };

    struct TLAS {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        RayTracingContext::ASBuffer backing = {};
        RayTracingContext::ASBuffer instanceBuffer = {};

        bool IsValid() const { return handle != VK_NULL_HANDLE; }
    };

    // Build / rebuild a TLAS from a list of instances.
    // Allocates all backing memory internally.
    // If `tlas.IsValid()`, the existing TLAS is destroyed and recreated.
    void BuildTLAS(TLAS& tlas,
        const std::vector<TLASInstance>& instances,
        RayTracingContext& ctx,
        VkCommandPool* pool);

    void DestroyTLAS(TLAS& tlas, RayTracingContext& ctx);

} // namespace Simple3D