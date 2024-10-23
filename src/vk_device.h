#pragma once

#include <VkBootstrap.h>
#include "vk_types.h"
#include "vk_initializers.h"

#include "vk_mem_alloc.h"


class VulkanDevice {
public:
	enum queueType {
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

	void immediateSubmit(std::function<void(VkCommandBuffer* commandBuffer)>&& function, queueType queueType);

	void destroy();
	void init(vkb::PhysicalDevice physicalDevice, VkInstance instance);

};