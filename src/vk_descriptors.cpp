#include "vk_descriptors.h"


void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newLayoutBinding{};
    newLayoutBinding.binding = binding;
    newLayoutBinding.descriptorCount = 1;
    newLayoutBinding.descriptorType = type;

    bindings.push_back(newLayoutBinding);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& binding : bindings) {
        binding.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutCreateInfo.pNext = pNext;

    layoutCreateInfo.pBindings = bindings.data();
    layoutCreateInfo.bindingCount = (uint32_t)bindings.size();
    layoutCreateInfo.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &set));

    return set;
}



VkDescriptorPool DescriptorAllocatorGrowable::getPool(VulkanDevice* device)
{
    VkDescriptorPool newPool;
    if (readyPools.size() != 0) {
        newPool = readyPools.back();
        readyPools.pop_back();
    }
    else {
        //need to create a new pool
        newPool = createPool(device, setsPerPool, ratios);

        setsPerPool = setsPerPool * 1.5;
        if (setsPerPool > 4092) {
            setsPerPool = 4092;
        }
    }

    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::createPool(VulkanDevice* device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * setCount)
            });
    }

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = 0;
    poolInfo.maxSets = setCount;
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(device->logicalDevice, &poolInfo, nullptr, &newPool);
    return newPool;
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VulkanDevice* device, VkDescriptorSetLayout layout, void* pNext)
{
    //get or create a pool to allocate from
    VkDescriptorPool poolToUse = getPool(device);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = pNext;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &ds);

    //allocation failed. Try again
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

        fullPools.push_back(poolToUse);

        poolToUse = getPool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &ds));
    }

    readyPools.push_back(poolToUse);
    return ds;
}

void DescriptorAllocatorGrowable::init(VulkanDevice* device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
    ratios.clear();

    for (auto r : poolRatios) {
        ratios.push_back(r);
    }

    VkDescriptorPool newPool = createPool(device, maxSets, poolRatios);

    setsPerPool = maxSets * 1.5; //grow it next allocation

    readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clearPools(VulkanDevice* device)
{
    for (auto p : readyPools) {
        vkResetDescriptorPool(device->logicalDevice, p, 0);
    }
    for (auto p : fullPools) {
        vkResetDescriptorPool(device->logicalDevice, p, 0);
        readyPools.push_back(p);
    }
    fullPools.clear();
}

void DescriptorAllocatorGrowable::destroyPools(VulkanDevice* device)
{
    for (auto p : readyPools) {
        vkDestroyDescriptorPool(device->logicalDevice, p, nullptr);
    }
    readyPools.clear();
    for (auto p : fullPools) {
        vkDestroyDescriptorPool(device->logicalDevice, p, nullptr);
    }
    fullPools.clear();
}




void DescriptorWriter::writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
    VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size
        });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;

    writes.push_back(write);
}

void DescriptorWriter::writeImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
{
    VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout
        });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &info;

    writes.push_back(write);
}

void DescriptorWriter::clear()
{
    imageInfos.clear();
    writes.clear();
    bufferInfos.clear();
}

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}

