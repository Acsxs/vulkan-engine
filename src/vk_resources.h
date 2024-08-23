#pragma once 
#include <vulkan/vulkan.h>
#include "vk_initializers.h"
#include <vk_mem_alloc.h>



struct VulkanImage {
    VkExtent3D imageExtent;
    VkFormat imageFormat;
    VkImageLayout imageLayout;

    VkImage image;
    VkImageView imageView;

    void transitionImage(
        VkCommandBuffer* commandBuffer,
        VkImageLayout newLayout,
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        VkPipelineStageFlagBits2 srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VkPipelineStageFlagBits2 dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VkAccessFlagBits2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        VkAccessFlagBits2 dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT
    );
    void copyToImage(VkCommandBuffer* commandBuffer, VkImage* destination, VkExtent3D dstSize, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
    void copyToImage(VkCommandBuffer* commandBuffer, VkImage* destination, VkExtent2D dstSize, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
};

struct AllocatedImage : VulkanImage {
    VmaAllocation allocation;
};



struct VulkanBuffer {
    VkBuffer buffer;

    void copyToBuffer(
        VkCommandBuffer* commandBuffer,
        VulkanBuffer dstBufffer,
        size_t size,
        uint32_t srcOffset = 0,
        uint32_t dstOffset = 0
    );

    void copyToImage(
        VkCommandBuffer* commandBuffer,
        VulkanImage image,
        uint32_t srcOffset,
        uint32_t bufferRowLength,
        uint32_t imageHeight,
        VkImageAspectFlags aspect,
        VkExtent3D size
    );
};

struct AllocatedBuffer : VulkanBuffer {
    VmaAllocation allocation;
    VmaAllocationInfo info;

    void uploadData(void* data, size_t dataSize) { memcpy(info.pMappedData, data, dataSize); };
};


