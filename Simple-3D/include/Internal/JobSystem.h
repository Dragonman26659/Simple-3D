#pragma once
#include <functional>
#include <vector>
#include <vulkan/vulkan.h>

namespace Simple3D {

    // ---------------------------------------------------------------
    // Abstract interface — implement this with your threading library
    // ---------------------------------------------------------------
    class IJobSystem {
    public:
        virtual ~IJobSystem() = default;

        using Job = std::function<void()>;

        // Submit N independent jobs and block until all complete.
        // The implementation decides thread count, work-stealing, etc.
        virtual void ParallelFor(std::vector<Job> jobs) = 0;

        // Submit a single fire-and-forget job (non-blocking).
        virtual void Submit(Job job) = 0;
    };

    // ---------------------------------------------------------------
    // Thread-local secondary command buffer context passed to each job
    // ---------------------------------------------------------------
    struct CommandRecordContext {
        VkCommandBuffer secondaryCmd = VK_NULL_HANDLE;
        uint32_t        frameIndex = 0;
        uint32_t        imageIndex = 0;
    };

} // namespace Simple3D