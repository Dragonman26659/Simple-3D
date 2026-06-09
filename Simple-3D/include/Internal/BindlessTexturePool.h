#pragma once
// BindlessTexturePool.h
// A single large descriptor array (set = 0, binding = 0) of
// VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER covering every texture in the
// renderer.  Shaders index into it with a push constant or storage-buffer
// material ID, removing per-draw descriptor set rebinding.
//
// Requirements:
//   VK_EXT_descriptor_indexing features must be enabled in the device:
//     - runtimeDescriptorArray
//     - descriptorBindingPartiallyBound
//     - descriptorBindingSampledImageUpdateAfterBind
//
// Usage:
//   BindlessTexturePool pool;
//   pool.Init(device, maxTextures);
//   uint32_t slot = pool.Register(view, sampler);
//   pool.Flush();                          // call once per frame after all Registers
//   vkCmdBindDescriptorSets(..., pool.GetSet(), ...);

#include "SimpleCore.h"
#include "Internal/Tools.h"

namespace Simple3D {

    class BindlessTexturePool {
    public:
        static constexpr uint32_t k_Invalid = UINT32_MAX;

        BindlessTexturePool() = default;
        ~BindlessTexturePool() = default;

        // Call once after device creation.
        // maxTextures: the upper bound on simultaneously-live textures.
        // A reasonable default for most games is 4096–16384.
        void Init(VkDevice device, uint32_t maxTextures = 4096);

        void Shutdown();

        // Register a texture view + sampler.
        // Returns the slot index to pass to shaders.
        // Thread-safe: can be called from asset-loading threads.
        uint32_t Register(VkImageView view, VkSampler sampler);

        // Free a slot so it can be reused.
        void Unregister(uint32_t slot);

        // Push all pending writes to the GPU descriptor set.
        // Call once per frame before any draws that use the bindless set.
        void Flush();

        VkDescriptorSet       GetSet()    const { return m_Set; }
        VkDescriptorSetLayout GetLayout() const { return m_Layout; }

        bool IsInitialised() const { return m_Pool != VK_NULL_HANDLE; }

    private:
        VkDevice              m_Device = VK_NULL_HANDLE;
        VkDescriptorPool      m_Pool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
        VkDescriptorSet       m_Set = VK_NULL_HANDLE;

        uint32_t              m_MaxTextures = 0;

        struct PendingWrite {
            uint32_t     slot;
            VkImageView  view;
            VkSampler    sampler;
        };

        std::mutex               m_Mutex;
        std::vector<uint32_t>    m_FreeSlots;    // recycled indices
        uint32_t                 m_NextSlot = 0; // monotonically growing if no free slots
        std::vector<PendingWrite> m_Pending;
    };

} // namespace Simple3D