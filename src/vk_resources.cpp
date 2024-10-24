#include "vk_resources.h"

void VulkanImage::transitionImage(
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



void VulkanImage::copyToImage(VkCommandBuffer* commandBuffer, VkImage* destination, VkExtent3D dstSize, VkImageAspectFlags aspectMask)
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

void VulkanImage::copyToImage(VkCommandBuffer* commandBuffer, VkImage* destination, VkExtent2D dstSize, VkImageAspectFlags aspectMask)
{
    VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

    blitRegion.srcOffsets[1].x = imageExtent.width;
    blitRegion.srcOffsets[1].y = imageExtent.height;
    blitRegion.srcOffsets[1].z = 1;

    blitRegion.dstOffsets[1].x = dstSize.width;
    blitRegion.dstOffsets[1].y = dstSize.height;
    blitRegion.dstOffsets[1].z = 1;

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

void VulkanBuffer::copyToBuffer(VkCommandBuffer* commandBuffer, VulkanBuffer* dstBuffer, size_t size, uint32_t srcOffset, uint32_t dstOffset) {
    VkBufferCopy bufferCopy{ 0 };
    bufferCopy.dstOffset = srcOffset;
    bufferCopy.srcOffset = dstOffset;
    bufferCopy.size = size;

    vkCmdCopyBuffer(*commandBuffer, buffer, dstBuffer->buffer, 1, &bufferCopy);
}

void VulkanBuffer::copyToImage(VkCommandBuffer* commandBuffer, VulkanImage* image, uint32_t srcOffset, uint32_t bufferRowLength, uint32_t bufferImageHeight, VkImageAspectFlags aspect, VkExtent3D size) {
    image->transitionImage(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = srcOffset;
    copyRegion.bufferRowLength = bufferRowLength;
    copyRegion.bufferImageHeight = bufferImageHeight;

    copyRegion.imageSubresource.aspectMask = aspect;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = size;

    vkCmdCopyBufferToImage(*commandBuffer, buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
}



void AllocatedBuffer::init(VulkanDevice* device, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {

    // allocate buffer
    VkBufferCreateInfo bufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.size = allocSize;

    bufferCreateInfo.usage = usage;

    VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
    bufferAllocationCreateInfo.usage = memoryUsage;
    bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(device->allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &buffer, &allocation, &info));
    isInitialized = true;
}

void AllocatedBuffer::destroy(VulkanDevice* device) {
    vmaDestroyBuffer(device->allocator, buffer, allocation);
}


void AllocatedImage::init(VulkanDevice* device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool mipmapped)
{
    imageFormat = format;
    imageExtent = size;
    imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageCreateInfo img_info = vkinit::ImageCreateInfo(format, usage, size);
    if (mipmapped) {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(device->allocator, &img_info, &allocinfo, &image, &allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    //VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    //if (format == VK_FORMAT_D32_SFLOAT) {
    //	aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    //}

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::ImageViewCreateInfo(format, image, aspect);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(device->logicalDevice, &view_info, nullptr, &imageView));
    isInitialized = true;
}

void AllocatedImage::init(VulkanDevice* device, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool mipmapped)
{
    size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadBuffer = {};
    uploadBuffer.init(device, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    uploadBuffer.uploadData(data, dataSize);

    init(device, size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, aspect, mipmapped);

    device->immediateSubmit([&](VkCommandBuffer* commandBuffer) { uploadBuffer.copyToImage(commandBuffer, this, 0, 0, 0, aspect, size); });

    uploadBuffer.destroy(device);
}

void AllocatedImage::destroy(VulkanDevice* device)
{
    vkDestroyImageView(device->logicalDevice, imageView, nullptr);
    vmaDestroyImage(device->allocator, image, allocation);
}