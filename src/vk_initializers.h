#pragma once

#include <vk_types.h>

namespace vkinit {
VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1);


VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);
VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd);

VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

VkSubmitInfo2 SubmitInfo2(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);
VkPresentInfoKHR PresentInfoKHR();

VkRenderingAttachmentInfo RenderingAttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

VkRenderingAttachmentInfo DepthAttachmentInfo(VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

VkRenderingInfo RenderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment);

VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspectMask);

VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutBinding* bindings, uint32_t bindingCount);
VkWriteDescriptorSet WriteDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);
VkWriteDescriptorSet WriteDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);
VkDescriptorBufferInfo DescriptorBufferInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

VkImageCreateInfo ImageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
VkImageViewCreateInfo ImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo();
VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char * entry = "main");
} 
