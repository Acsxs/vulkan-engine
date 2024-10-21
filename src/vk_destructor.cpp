#include "vk_destructor.h"
#include <iostream>

void ResourceDestructor::destroy(VulkanDevice* vulkanDevice, VkFence* fence) {
	if (fence != nullptr) {
		VK_CHECK(vkWaitForFences(vulkanDevice->logicalDevice, 1, fence, true, 1000000000));
	}

	for (AllocatedImage* image : images) {
		vulkanDevice->destroyImage(*image);
	}
	for (AllocatedBuffer* buffer : buffers) {
		vulkanDevice->destroyBuffer(*buffer);
	}
	for (VkSampler* sampler : samplers) {
		vkDestroySampler(vulkanDevice->logicalDevice, *sampler, nullptr);
	}
	for (VulkanPipeline* pipeline : pipelines) {
		pipeline->destroy(vulkanDevice);
	}

}