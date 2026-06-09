// BindlessTexturePool.cpp
#include "Internal/BindlessTexturePool.h"
#include <stdexcept>

namespace Simple3D {

    void BindlessTexturePool::Init(VkDevice device, uint32_t maxTextures)
    {
        m_Device = device;
        m_MaxTextures = maxTextures;

        // ── Descriptor set layout with UPDATE_AFTER_BIND ───────────────────────
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = maxTextures;
        binding.stageFlags = VK_SHADER_STAGE_ALL;

        // Flags: array is partially bound (unused slots are VK_NULL_HANDLE),
        // and the set can be updated between command buffer submissions.
        VkDescriptorBindingFlags bindingFlags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsCI{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
        flagsCI.bindingCount = 1;
        flagsCI.pBindingFlags = &bindingFlags;

        VkDescriptorSetLayoutCreateInfo layoutCI{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layoutCI.pNext = &flagsCI;
        layoutCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutCI.bindingCount = 1;
        layoutCI.pBindings = &binding;

        if (vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &m_Layout) != VK_SUCCESS)
            throw std::runtime_error("BindlessTexturePool: failed to create descriptor set layout");

        // ── Descriptor pool ────────────────────────────────────────────────────
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = maxTextures;

        VkDescriptorPoolCreateInfo poolCI{
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolCI.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolCI.maxSets = 1;
        poolCI.poolSizeCount = 1;
        poolCI.pPoolSizes = &poolSize;

        if (vkCreateDescriptorPool(device, &poolCI, nullptr, &m_Pool) != VK_SUCCESS)
            throw std::runtime_error("BindlessTexturePool: failed to create descriptor pool");

        // ── Allocate the single descriptor set ────────────────────────────────
        VkDescriptorSetAllocateInfo allocInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = m_Pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_Layout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &m_Set) != VK_SUCCESS)
            throw std::runtime_error("BindlessTexturePool: failed to allocate descriptor set");
    }

    void BindlessTexturePool::Shutdown()
    {
        if (m_Pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(m_Device, m_Pool, nullptr);
        if (m_Layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(m_Device, m_Layout, nullptr);

        m_Pool = VK_NULL_HANDLE;
        m_Layout = VK_NULL_HANDLE;
        m_Set = VK_NULL_HANDLE;
    }

    uint32_t BindlessTexturePool::Register(VkImageView view, VkSampler sampler)
    {
        std::lock_guard<std::mutex> lk(m_Mutex);

        uint32_t slot;
        if (!m_FreeSlots.empty()) {
            slot = m_FreeSlots.back();
            m_FreeSlots.pop_back();
        }
        else {
            if (m_NextSlot >= m_MaxTextures)
                throw std::runtime_error("BindlessTexturePool: texture slot limit reached");
            slot = m_NextSlot++;
        }

        m_Pending.push_back({ slot, view, sampler });
        return slot;
    }

    void BindlessTexturePool::Unregister(uint32_t slot)
    {
        if (slot == k_Invalid) return;
        std::lock_guard<std::mutex> lk(m_Mutex);
        m_FreeSlots.push_back(slot);
        // We don't need to null-out the slot in the descriptor set because
        // VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT allows unused slots.
    }

    void BindlessTexturePool::Flush()
    {
        std::vector<PendingWrite> pending;
        {
            std::lock_guard<std::mutex> lk(m_Mutex);
            pending.swap(m_Pending);
        }

        if (pending.empty()) return;

        std::vector<VkWriteDescriptorSet>  writes;
        std::vector<VkDescriptorImageInfo> imageInfos;
        writes.reserve(pending.size());
        imageInfos.reserve(pending.size());

        for (const auto& pw : pending) {
            VkDescriptorImageInfo& img = imageInfos.emplace_back();
            img.imageView = pw.view;
            img.sampler = pw.sampler;
            img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet& w = writes.emplace_back();
            w = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            w.dstSet = m_Set;
            w.dstBinding = 0;
            w.dstArrayElement = pw.slot;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &imageInfos.back();
        }

        vkUpdateDescriptorSets(m_Device,
            static_cast<uint32_t>(writes.size()), writes.data(),
            0, nullptr);
    }

} // namespace Simple3D