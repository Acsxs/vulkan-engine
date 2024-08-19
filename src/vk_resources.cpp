#include <vk_resources.h>


void AllocatedImage::transitionImage(
    VkCommandBuffer* commandBuffer,
    VkImageLayout newLayout,
    VkImageAspectFlags aspectMask,
    VkPipelineStageFlagBits2 srcStageMask,
    VkPipelineStageFlagBits2 dstStageMask,
    VkAccessFlagBits2 srcAccessMask,
    VkAccessFlagBits2 dstAccessMask
)
{
    VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    imageBarrier.pNext = nullptr;

    /*imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;*/

    imageBarrier.srcStageMask = srcStageMask;
    imageBarrier.dstStageMask = dstStageMask;
    imageBarrier.srcAccessMask = srcAccessMask;
    imageBarrier.dstAccessMask = dstAccessMask; 

    imageBarrier.oldLayout = imageLayout;
    imageBarrier.newLayout = newLayout;

    //VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = vkinit::ImageSubresourceRange(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pNext = nullptr;

    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;
  

    vkCmdPipelineBarrier2(*commandBuffer, &dependencyInfo);

    imageLayout = newLayout;
}

void AllocatedImage::copyToImage(VkCommandBuffer* commandBuffer, VkImage* destination, VkExtent3D dstSize, VkImageAspectFlags aspectMask)
{
    VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

    blitRegion.srcOffsets[1].x = imageExtent.width;
    blitRegion.srcOffsets[1].y = imageExtent.height;
    blitRegion.srcOffsets[1].z = imageExtent.depth;

    blitRegion.dstOffsets[1].x = dstSize.width;
    blitRegion.dstOffsets[1].y = dstSize.height;
    blitRegion.dstOffsets[1].z = dstSize.depth;

    blitRegion.srcSubresource.aspectMask = aspectMask;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;

    blitRegion.dstSubresource.aspectMask = aspectMask;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
    blitInfo.dstImage = *destination;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.srcImage = image;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.filter = VK_FILTER_LINEAR;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    vkCmdBlitImage2(*commandBuffer, &blitInfo);
}

void AllocatedBuffer::copyToBuffer(VkCommandBuffer* commandBuffer, AllocatedBuffer dstBuffer, size_t size, uint32_t srcOffset, uint32_t dstOffset) {
    VkBufferCopy bufferCopy{ 0 };
    bufferCopy.dstOffset = srcOffset;
    bufferCopy.srcOffset = dstOffset;
    bufferCopy.size = size;

    vkCmdCopyBuffer(*commandBuffer, buffer, dstBuffer.buffer, 1, &bufferCopy);
}

void AllocatedBuffer::copyToImage(VkCommandBuffer* commandBuffer, AllocatedImage image, uint32_t srcOffset, uint32_t bufferRowLength, uint32_t bufferImageHeight, VkImageAspectFlags aspect, size_t size) {
    image.transitionImage(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = srcOffset;
    copyRegion.bufferRowLength = bufferRowLength;
    copyRegion.bufferImageHeight = bufferImageHeight;

    copyRegion.imageSubresource.aspectMask = aspect;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = size;

    vkCmdCopyBufferToImage(*commandBuffer, buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
}
