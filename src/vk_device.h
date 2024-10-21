#pragma once

#include <VkBootstrap.h>
#include "vk_types.h"
#include "vk_initializers.h"



class VulkanDevice {
public:
	enum queueIndices {
		GRAPHICS,
		TRANSFER,
		COMPUTE
	};
	VmaAllocator allocator;

	VkFence immFence;
	VkCommandBuffer immCommandBuffer;
	VkCommandPool immCommandPool;

	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
	uint32_t queueFamilies[3];
	std::shared_ptr<VkQueue> queues[3];

	uint32_t getQueueFamilyIndex(uint8_t queueIndex) { return queueFamilies[queueIndex]; };

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, bool mipmapped = false);
	AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, bool mipmapped = false);
	void destroyImage(const AllocatedImage& img);

	void immediateSubmit(std::function<void(VkCommandBuffer* commandBuffer)>&& function);

	void destroy();
	void init(vkb::PhysicalDevice physicalDevice, VkInstance instance);

};