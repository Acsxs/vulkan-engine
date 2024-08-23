#pragma once

#include "vk_types.h"
#include "vk_resources.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_device.h"

struct ResourceDestructor {
	std::vector<AllocatedImage*> images;
	std::vector<AllocatedBuffer*> buffers;
	std::vector<VkSampler*> samplers;
	std::vector<VulkanPipeline*> pipelines;
	
	void destroy(VulkanDevice* vulkanDevice, VkFence* fence);
};