#pragma once
#include "vk_device.h"


struct DescriptorLayoutBuilder {

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void addBinding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};


struct DescriptorAllocatorGrowable {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    void init(VulkanDevice* device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
    void clearPools(VulkanDevice* device);
    void destroyPools(VulkanDevice* device);

    VkDescriptorSet allocate(VulkanDevice* device, VkDescriptorSetLayout layout, void* pNext = nullptr);
private:
    VkDescriptorPool getPool(VulkanDevice* device);
    VkDescriptorPool createPool(VulkanDevice* device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> fullPools;
    std::vector<VkDescriptorPool> readyPools;
    uint32_t setsPerPool;

};


struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void writeImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
    void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

    void clear();
    void updateSet(VkDevice device, VkDescriptorSet set);
};