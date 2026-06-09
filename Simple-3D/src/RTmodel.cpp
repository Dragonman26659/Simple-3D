// Model_RT.cpp
#include "Internal/RT/RTModel.h"
#include <stdexcept>

namespace Simple3D {

    // ── Internal: fill build geometry from model buffers ─────────────────────────
    static void FillBLASGeometry(
        Model& model,
        RayTracingContext& ctx,
        VkAccelerationStructureGeometryKHR& geom,
        VkAccelerationStructureBuildRangeInfoKHR& rangeInfo)
    {
        VkDeviceAddress vertexAddr =
            ctx.GetBufferAddress(model.GetVertexBuffer());
        VkDeviceAddress indexAddr =
            ctx.GetBufferAddress(model.GetIndexBuffer());

        VkAccelerationStructureGeometryTrianglesDataKHR triData{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
        triData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; // position is first 3 floats of Vertex
        triData.vertexData.deviceAddress = vertexAddr;
        triData.vertexStride = sizeof(Vertex);
        triData.maxVertex = static_cast<uint32_t>(model.GetVerticies().size()) - 1;
        triData.indexType = VK_INDEX_TYPE_UINT32;
        triData.indexData.deviceAddress = indexAddr;

        geom = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geom.geometry.triangles = triData;
        geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        rangeInfo = {};
        rangeInfo.primitiveCount = static_cast<uint32_t>(model.Indices.size()) / 3;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.firstVertex = 0;
        rangeInfo.transformOffset = 0;
    }

    // ── BuildBLAS ─────────────────────────────────────────────────────────────────
    BLAS BuildBLAS(Model& model,
        RayTracingContext& ctx,
        VkCommandPool* pool,
        bool allowUpdate)
    {
        if (!ctx.IsAvailable()) return {};
        if (!model.hasBuffer())
            throw std::runtime_error("BuildBLAS: model buffers not yet created");

        const auto& pfn = ctx.GetPFN();
        VkDevice device = ctx.GetDevice();

        // The vertex buffer needs VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT and
        // VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR.
        // This is set in Model::CreateVertexBuffer() when RT is enabled — see note
        // in Model.h about the RayTracingContext* parameter.

        // ── Geometry ─────────────────────────────────────────────────────────────
        VkAccelerationStructureGeometryKHR geom{};
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        FillBLASGeometry(model, ctx, geom, rangeInfo);

        // ── Build flags ──────────────────────────────────────────────────────────
        VkBuildAccelerationStructureFlagsKHR flags =
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        if (allowUpdate)
            flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = flags;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geom;

        uint32_t primCount = rangeInfo.primitiveCount;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        pfn.getBuildSizes(device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &primCount, &sizeInfo);

        // ── Create backing buffer + AS handle ────────────────────────────────────
        BLAS blas{};
        blas.backing = ctx.CreateASBuffer(sizeInfo.accelerationStructureSize);

        VkAccelerationStructureCreateInfoKHR asCI{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        asCI.buffer = blas.backing.buffer;
        asCI.size = sizeInfo.accelerationStructureSize;
        asCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        pfn.createAS(device, &asCI, nullptr, &blas.handle);

        // ── Scratch buffer ───────────────────────────────────────────────────────
        RayTracingContext::ASBuffer scratch = ctx.CreateASBuffer(
            sizeInfo.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        // ── Build ────────────────────────────────────────────────────────────────
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.dstAccelerationStructure = blas.handle;
        buildInfo.scratchData.deviceAddress = scratch.address;

        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

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

        // Barrier so the BLAS is visible to any subsequent TLAS build in the same submit.
        VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0, 1, &barrier, 0, nullptr, 0, nullptr);

        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkFence fence{};
        vkCreateFence(device, &fci, nullptr, &fence);

        VkQueue gfxQueue{};
        vkGetDeviceQueue(device, 0, 0, &gfxQueue); // replace with Device::getVKgraphicsQueue()

        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        vkQueueSubmit(gfxQueue, 1, &si, fence);
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, *pool, 1, &cmd);

        ctx.DestroyASBuffer(scratch);
        return blas;
    }

    // ── DestroyBLAS ───────────────────────────────────────────────────────────────
    void DestroyBLAS(BLAS& blas, RayTracingContext& ctx)
    {
        if (!blas.IsValid()) return;
        ctx.GetPFN().destroyAS(ctx.GetDevice(), blas.handle, nullptr);
        blas.handle = VK_NULL_HANDLE;
        ctx.DestroyASBuffer(blas.backing);
    }

    // ── RefitBLAS ─────────────────────────────────────────────────────────────────
    void RefitBLAS(BLAS& blas,
        Model& model,
        RayTracingContext& ctx,
        VkCommandPool* pool)
    {
        if (!ctx.IsAvailable() || !blas.IsValid()) return;

        const auto& pfn = ctx.GetPFN();
        VkDevice device = ctx.GetDevice();

        VkAccelerationStructureGeometryKHR geom{};
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        FillBLASGeometry(model, ctx, geom, rangeInfo);

        uint32_t primCount = rangeInfo.primitiveCount;
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR |
            VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geom;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        pfn.getBuildSizes(device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &primCount, &sizeInfo);

        RayTracingContext::ASBuffer scratch = ctx.CreateASBuffer(
            sizeInfo.updateScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
        buildInfo.srcAccelerationStructure = blas.handle;
        buildInfo.dstAccelerationStructure = blas.handle;
        buildInfo.scratchData.deviceAddress = scratch.address;

        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

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

        VkQueue gfxQueue{};
        vkGetDeviceQueue(device, 0, 0, &gfxQueue);

        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        vkQueueSubmit(gfxQueue, 1, &si, fence);
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, *pool, 1, &cmd);

        ctx.DestroyASBuffer(scratch);
    }

} // namespace Simple3D