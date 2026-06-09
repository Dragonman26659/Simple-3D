// RayTracingContext.cpp
#include "Internal/RTcontext.h"
#include "Internal/Tools.h"     // beginSingleTimeCommands / endSingleTimeCommands
#include <stdexcept>
#include <cstring>
#include <vector>
#include <algorithm>

namespace Simple3D {

    // ── Required device extensions for RT ────────────────────────────────────────
    static constexpr const char* k_RequiredRTExtensions[] = {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    };
    static constexpr const char* k_FullPipelineExtension =
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME;

    // ── CheckExtensionSupport ─────────────────────────────────────────────────────
    bool RayTracingContext::CheckExtensionSupport(VkPhysicalDevice physDevice)
    {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available(count);
        vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &count, available.data());

        auto has = [&](const char* name) {
            return std::any_of(available.begin(), available.end(),
                [name](const VkExtensionProperties& p) {
                    return std::strcmp(p.extensionName, name) == 0;
                });
            };

        for (auto ext : k_RequiredRTExtensions)
            if (!has(ext)) return false;
        return true;
    }

    // ── Init ──────────────────────────────────────────────────────────────────────
    void RayTracingContext::Init(VkInstance instance,
        VkPhysicalDevice physDevice,
        VkDevice device)
    {
        m_Device = device;

        // 1. Check extension presence at the instance level.
        if (!CheckExtensionSupport(physDevice)) {
            m_Support = {};
            return;
        }

        // 2. Check feature bits — all required features must be enabled in the
        //    device feature chain.  The engine's Device class must have requested
        //    these in its pNext chain when calling vkCreateDevice (see Device.cpp).
        VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        asFeatures.pNext = &bdaFeatures;

        VkPhysicalDeviceRayQueryFeaturesKHR rqFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
        rqFeatures.pNext = &asFeatures;

        VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        features2.pNext = &rqFeatures;

        vkGetPhysicalDeviceFeatures2(physDevice, &features2);

        m_Support.bufferDeviceAddress = bdaFeatures.bufferDeviceAddress;
        m_Support.accelerationStructure = asFeatures.accelerationStructure;
        m_Support.rayQuery = rqFeatures.rayQuery;

        // Descriptor indexing (needed for bindless).
        VkPhysicalDeviceDescriptorIndexingFeatures diFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
        VkPhysicalDeviceFeatures2 diQuery{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        diQuery.pNext = &diFeatures;
        vkGetPhysicalDeviceFeatures2(physDevice, &diQuery);
        m_Support.descriptorIndexing = diFeatures.runtimeDescriptorArray &&
            diFeatures.descriptorBindingPartiallyBound;

        // Optional full-pipeline support.
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extCount, exts.data());
        m_Support.rayTracingPipeline = std::any_of(exts.begin(), exts.end(),
            [](const VkExtensionProperties& p) {
                return std::strcmp(p.extensionName, k_FullPipelineExtension) == 0;
            });

        if (!m_Support.SupportsRayQueryInFragmentShader())
            return; // extensions present but features not enabled — nothing to load

        LoadPFNTable();
    }

    // ── LoadPFNTable ──────────────────────────────────────────────────────────────
#define LOAD_PFN(name, field)                                                   \
    m_PFN.field = reinterpret_cast<PFN_##name>(                                 \
        vkGetDeviceProcAddr(m_Device, #name));                                  \
    if (!m_PFN.field) {                                                         \
        m_Support = {};                                                         \
        return;                                                                 \
    }

    void RayTracingContext::LoadPFNTable()
    {
        LOAD_PFN(vkCreateAccelerationStructureKHR, createAS)
            LOAD_PFN(vkDestroyAccelerationStructureKHR, destroyAS)
            LOAD_PFN(vkGetAccelerationStructureBuildSizesKHR, getBuildSizes)
            LOAD_PFN(vkCmdBuildAccelerationStructuresKHR, cmdBuildAS)
            LOAD_PFN(vkGetAccelerationStructureDeviceAddressKHR, getDeviceAddress)
            LOAD_PFN(vkCmdWriteAccelerationStructuresPropertiesKHR, cmdWriteProps)
    }
#undef LOAD_PFN

    // ── CreateASBuffer ────────────────────────────────────────────────────────────
    RayTracingContext::ASBuffer RayTracingContext::CreateASBuffer(
        VkDeviceSize size, VkBufferUsageFlags extraUsage)
    {
        ASBuffer result{};

        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size = size;
        bci.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            extraUsage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (vmaCreateBuffer(Allocator::Get(), &bci, &aci,
            &result.buffer, &result.alloc.handle, &result.alloc.info)
            != VK_SUCCESS)
            throw std::runtime_error("RayTracingContext: failed to create AS buffer");

        result.address = GetBufferAddress(result.buffer);
        return result;
    }

    void RayTracingContext::DestroyASBuffer(ASBuffer& buf)
    {
        if (buf.buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(Allocator::Get(), buf.buffer, buf.alloc.handle);
            buf.buffer = VK_NULL_HANDLE;
            buf.alloc = {};
            buf.address = 0;
        }
    }

    VkDeviceAddress RayTracingContext::GetBufferAddress(VkBuffer buffer) const
    {
        VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        info.buffer = buffer;
        return vkGetBufferDeviceAddress(m_Device, &info);
    }

    // ── BuildTLAS ─────────────────────────────────────────────────────────────────
    void BuildTLAS(TLAS& tlas,
        const std::vector<TLASInstance>& instances,
        RayTracingContext& ctx,
        VkCommandPool* pool)
    {
        if (!ctx.IsAvailable())
            throw std::runtime_error("BuildTLAS: ray tracing is not available");

        const auto& pfn = ctx.GetPFN();
        VkDevice device = ctx.GetDevice();

        // Destroy the previous TLAS if it existed.
        DestroyTLAS(tlas, ctx);

        // ── 1. Convert TLASInstance → VkAccelerationStructureInstanceKHR ──────────
        std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
        vkInstances.reserve(instances.size());

        for (const auto& inst : instances) {
            VkAccelerationStructureInstanceKHR vkInst{};

            // VK wants a row-major 3x4 transform (top 3 rows of a 4x4 matrix).
            const glm::mat4& m = glm::transpose(inst.transform);
            std::memcpy(&vkInst.transform, &m, sizeof(vkInst.transform));

            vkInst.instanceCustomIndex = inst.instanceID;
            vkInst.mask = inst.mask;
            vkInst.instanceShaderBindingTableRecordOffset = inst.hitGroupIndex;
            vkInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

            VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
            addrInfo.accelerationStructure = inst.blas;
            vkInst.accelerationStructureReference =
                pfn.getDeviceAddress(device, &addrInfo);

            vkInstances.push_back(vkInst);
        }

        // ── 2. Upload instance data to a device-visible buffer ─────────────────
        VkDeviceSize instBufferSize =
            sizeof(VkAccelerationStructureInstanceKHR) * vkInstances.size();
        if (instBufferSize == 0) instBufferSize = sizeof(VkAccelerationStructureInstanceKHR);

        // Instance buffer needs to be device-addressable.
        VkBufferCreateInfo instBCI{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        instBCI.size = instBufferSize;
        instBCI.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VmaAllocationCreateInfo instACI{};
        instACI.usage = VMA_MEMORY_USAGE_AUTO;
        instACI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        vmaCreateBuffer(Allocator::Get(), &instBCI, &instACI,
            &tlas.instanceBuffer.buffer,
            &tlas.instanceBuffer.alloc.handle,
            &tlas.instanceBuffer.alloc.info);
        tlas.instanceBuffer.address = ctx.GetBufferAddress(tlas.instanceBuffer.buffer);

        void* mapped{};
        vmaMapMemory(Allocator::Get(), tlas.instanceBuffer.alloc.handle, &mapped);
        if (!vkInstances.empty())
            std::memcpy(mapped, vkInstances.data(),
                sizeof(VkAccelerationStructureInstanceKHR) * vkInstances.size());
        vmaUnmapMemory(Allocator::Get(), tlas.instanceBuffer.alloc.handle);

        // ── 3. Query build sizes ───────────────────────────────────────────────
        VkAccelerationStructureGeometryInstancesDataKHR instData{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
        instData.arrayOfPointers = VK_FALSE;
        instData.data.deviceAddress = tlas.instanceBuffer.address;

        VkAccelerationStructureGeometryKHR tlasGeom{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlasGeom.geometry.instances = instData;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &tlasGeom;

        uint32_t primCount = static_cast<uint32_t>(instances.size());

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        pfn.getBuildSizes(device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &primCount, &sizeInfo);

        // ── 4. Create backing buffer + AS handle ──────────────────────────────
        tlas.backing = ctx.CreateASBuffer(sizeInfo.accelerationStructureSize);

        VkAccelerationStructureCreateInfoKHR asCI{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        asCI.buffer = tlas.backing.buffer;
        asCI.size = sizeInfo.accelerationStructureSize;
        asCI.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        pfn.createAS(device, &asCI, nullptr, &tlas.handle);

        // ── 5. Scratch buffer ─────────────────────────────────────────────────
        RayTracingContext::ASBuffer scratch = ctx.CreateASBuffer(
            sizeInfo.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        // ── 6. Build on GPU ───────────────────────────────────────────────────
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.dstAccelerationStructure = tlas.handle;
        buildInfo.scratchData.deviceAddress = scratch.address;

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        rangeInfo.primitiveCount = primCount;
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

        // We need a device (for Device class pointer) to call begin/endSingleTimeCommands.
        // Since RayTracingContext only holds a VkDevice, we issue the cmd buffer manually.
        VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.commandPool = *pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd{};
        vkAllocateCommandBuffers(device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        pfn.cmdBuildAS(cmd, 1, &buildInfo, &pRange);

        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkFence fence{};
        vkCreateFence(device, &fci, nullptr, &fence);

        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;

        // NOTE: caller must ensure the graphics queue is accessible.
        // We expose a helper below; for now use the device queue via vkGetDeviceQueue.
        // In practice, RayTracingContext should store the queue — see Device integration notes.
        VkQueue gfxQueue{};
        // HACK: slot 0 family 0 — replace with ctx.GetGraphicsQueue() once integrated.
        vkGetDeviceQueue(device, 0, 0, &gfxQueue);

        vkQueueSubmit(gfxQueue, 1, &si, fence);
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, *pool, 1, &cmd);

        ctx.DestroyASBuffer(scratch);
    }

    void DestroyTLAS(TLAS& tlas, RayTracingContext& ctx)
    {
        if (!tlas.IsValid()) return;
        ctx.GetPFN().destroyAS(ctx.GetDevice(), tlas.handle, nullptr);
        tlas.handle = VK_NULL_HANDLE;
        ctx.DestroyASBuffer(tlas.backing);
        ctx.DestroyASBuffer(tlas.instanceBuffer);
    }

} // namespace Simple3D