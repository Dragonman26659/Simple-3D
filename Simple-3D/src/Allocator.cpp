// Allocator.cpp
// VMA_IMPLEMENTATION must appear exactly once in the project — here.
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "Internal/Allocator.h"
#include <stdexcept>
#include <cassert>

namespace Simple3D {

    // ── Static member definitions ─────────────────────────────────────────────────
    // Defined here (not inline) so there is exactly one instance per binary module.
    // Other modules receive the pointer via SetInstance().
    VmaAllocator                             Allocator::s_Allocator = VK_NULL_HANDLE;
    std::unordered_map<VkBuffer, Allocation> Allocator::s_BufferTable;
    std::mutex                               Allocator::s_BufferMutex;
    std::unordered_map<VkImage, Allocation> Allocator::s_ImageTable;
    std::mutex                               Allocator::s_ImageMutex;

    // ── Init ──────────────────────────────────────────────────────────────────────
    void Allocator::Init(VkInstance       instance,
        VkPhysicalDevice physicalDevice,
        VkDevice         device)
    {
        assert(instance != VK_NULL_HANDLE && "Allocator::Init — null VkInstance");
        assert(physicalDevice != VK_NULL_HANDLE && "Allocator::Init — null VkPhysicalDevice");
        assert(device != VK_NULL_HANDLE && "Allocator::Init — null VkDevice");

        VmaAllocatorCreateInfo info{};
        info.instance = instance;
        info.physicalDevice = physicalDevice;
        info.device = device;
        info.vulkanApiVersion = VK_API_VERSION_1_2;
        // Required for RT acceleration-structure scratch buffers.
        info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        if (vmaCreateAllocator(&info, &s_Allocator) != VK_SUCCESS)
            throw std::runtime_error("Allocator::Init — vmaCreateAllocator failed");
    }

    // ── Shutdown ──────────────────────────────────────────────────────────────────
    void Allocator::Shutdown()
    {
        if (s_Allocator == VK_NULL_HANDLE) return;

        {
            std::lock_guard<std::mutex> lk(s_BufferMutex);
            s_BufferTable.clear();
        }
        {
            std::lock_guard<std::mutex> lk(s_ImageMutex);
            s_ImageTable.clear();
        }

        vmaDestroyAllocator(s_Allocator);
        s_Allocator = VK_NULL_HANDLE;
    }

    // ── SetInstance ───────────────────────────────────────────────────────────────
    void Allocator::SetInstance(VmaAllocator allocator)
    {
        // Does not take ownership — the caller (Renderer) is responsible for lifetime.
        s_Allocator = allocator;
    }

    // ── Get ───────────────────────────────────────────────────────────────────────
    VmaAllocator Allocator::Get()
    {
        assert(s_Allocator != VK_NULL_HANDLE &&
            "Allocator::Get called before Init() or SetInstance()!");
        return s_Allocator;
    }

    // ── Legacy buffer table ───────────────────────────────────────────────────────
    void Allocator::RegisterLegacyBuffer(VkBuffer buffer, const Allocation& alloc)
    {
        std::lock_guard<std::mutex> lk(s_BufferMutex);
        s_BufferTable[buffer] = alloc;
    }

    void Allocator::UnregisterLegacyBuffer(VkBuffer buffer)
    {
        std::lock_guard<std::mutex> lk(s_BufferMutex);
        s_BufferTable.erase(buffer);
    }

    void Allocator::DestroyLegacyBuffer(VkBuffer buffer)
    {
        Allocation alloc{};
        {
            std::lock_guard<std::mutex> lk(s_BufferMutex);
            auto it = s_BufferTable.find(buffer);
            if (it == s_BufferTable.end()) return;
            alloc = it->second;
            s_BufferTable.erase(it);
        }
        if (alloc.handle != VK_NULL_HANDLE)
            vmaDestroyBuffer(s_Allocator, buffer, alloc.handle);
    }

    // ── Legacy image table ────────────────────────────────────────────────────────
    void Allocator::RegisterLegacyImage(VkImage image, const Allocation& alloc)
    {
        std::lock_guard<std::mutex> lk(s_ImageMutex);
        s_ImageTable[image] = alloc;
    }

    void Allocator::UnregisterLegacyImage(VkImage image)
    {
        std::lock_guard<std::mutex> lk(s_ImageMutex);
        s_ImageTable.erase(image);
    }

    void Allocator::DestroyLegacyImage(VkImage image)
    {
        Allocation alloc{};
        {
            std::lock_guard<std::mutex> lk(s_ImageMutex);
            auto it = s_ImageTable.find(image);
            if (it == s_ImageTable.end()) return;
            alloc = it->second;
            s_ImageTable.erase(it);
        }
        if (alloc.handle != VK_NULL_HANDLE)
            vmaDestroyImage(s_Allocator, image, alloc.handle);
    }

} // namespace Simple3D