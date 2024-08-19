#pragma once

#include <vk_types.h>
#include <VkBootstrap.h>
#include "vk_initializers.h"
#include "vk_resources.h"



class VulkanDevice {
public:
	enum queueIndices {
		GRAPHICS,
		TRANSFER,
		COMPUTE
	};
	VmaAllocator _allocator;

	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	VkPhysicalDevice _physicalDevice;
	VkDevice _logicalDevice;
	uint32_t _queueFamilies[3];
	VkQueue _queues[3];

	VkQueue getQueue(uint8_t queueIndex) { return _queues[queueIndex]; };
	uint32_t getQueueFamilyIndex(uint8_t queueIndex) { return _queueFamilies[queueIndex]; };

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, bool mipmapped = false);
	AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, bool mipmapped = false);
	void destroyImage(const AllocatedImage& img);

	void immediateSubmit(std::function<void(VkCommandBuffer* commandBuffer)>&& function);

	void destroy();
	void init(vkb::PhysicalDevice physicalDevice, VkInstance instance);

};