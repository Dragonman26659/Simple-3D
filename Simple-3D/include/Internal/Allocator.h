#pragma once
#include "SimpleCore.h"
#include <vma/vk_mem_alloc.h>
#include <mutex>
#include <unordered_map>

namespace Simple3D {

    // ── Allocation ────────────────────────────────────────────────────────────────
    struct Allocation {
        VmaAllocation     handle = VK_NULL_HANDLE;
        VmaAllocationInfo info = {};
    };

    // ── Allocator ─────────────────────────────────────────────────────────────────
    // Thin wrapper around VmaAllocator.
    //
    // Ownership: the Renderer creates and destroys the VmaAllocator.  Every other
    // translation unit (including those in other DLLs) calls Allocator::Get() which
    // returns the pointer that was registered via Allocator::SetInstance().
    //
    // Why pointer-based instead of inline static:
    //   An `inline static` member gets a separate copy per DLL/shared-library.
    //   The DLL that calls Init() initialises its copy; the EXE's copy stays null.
    //   Passing the pointer across the boundary keeps exactly one allocator alive.
    //
    class Allocator {
    public:
        // ── Lifecycle (called by Renderer only) ───────────────────────────────────

        // Creates a VmaAllocator and registers it as the global instance.
        static void Init(VkInstance       instance,
            VkPhysicalDevice physicalDevice,
            VkDevice         device);

        // Destroys the allocator and clears the global instance.
        static void Shutdown();

        // ── Cross-DLL plumbing ────────────────────────────────────────────────────

        // Returns the raw VmaAllocator pointer owned by this module.
        // Call this once from Renderer after Init() and pass the result to any
        // module (EXE, plugin DLL) that needs to allocate via VMA:
        //   Allocator::SetInstance(Allocator::GetRawAllocator());
        static VmaAllocator GetRawAllocator() { return s_Allocator; }

        // Register an externally-owned VmaAllocator as the global instance for
        // this module.  Call from every DLL/EXE that uses Allocator::Get() but
        // did not call Init().
        static void SetInstance(VmaAllocator allocator);

        // Returns the active VmaAllocator.  Asserts if not yet set.
        static VmaAllocator Get();

        static bool IsReady() { return s_Allocator != VK_NULL_HANDLE; }

        // ── Legacy buffer table ───────────────────────────────────────────────────
        static void RegisterLegacyBuffer(VkBuffer buffer, const Allocation& alloc);
        static void UnregisterLegacyBuffer(VkBuffer buffer);
        static void DestroyLegacyBuffer(VkBuffer buffer);

        // ── Legacy image table ────────────────────────────────────────────────────
        static void RegisterLegacyImage(VkImage image, const Allocation& alloc);
        static void UnregisterLegacyImage(VkImage image);
        static void DestroyLegacyImage(VkImage image);

    private:
        // These are defined in Allocator.cpp (exactly one TU), so they are true
        // singletons within that module.  SetInstance() lets other modules point
        // their own s_Allocator at the same underlying object.
        static VmaAllocator                          s_Allocator;
        static std::unordered_map<VkBuffer, Allocation> s_BufferTable;
        static std::mutex                            s_BufferMutex;
        static std::unordered_map<VkImage, Allocation> s_ImageTable;
        static std::mutex                            s_ImageMutex;
    };

} // namespace Simple3D