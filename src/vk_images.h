
#pragma once 
#include <vulkan/vulkan.h>
#include "vk_initializers.h"

namespace vkutil {

	void transitionImage(
        VkCommandBuffer* commandBuffer,
        VkImage* image,
        VkImageLayout currentLayout,
        VkImageLayout newLayout,
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        VkPipelineStageFlagBits2 srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VkPipelineStageFlagBits2 dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VkAccessFlagBits2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        VkAccessFlagBits2 dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT
    );

    void copyImageToImage(VkCommandBuffer* commandBuffer, VkImage source, VkImage* destination, VkExtent2D srcSize, VkExtent2D dstSize, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
}