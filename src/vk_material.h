#pragma once

#include "vk_types.h"
#include "vk_resources.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_device.h"






enum class MaterialPassType: uint8_t {
	MainColour,
	Transparent,
	Other
};


struct MaterialInstance {
	VulkanPipeline* pipeline;
	VkDescriptorSet materialDescriptors;
	MaterialPassType pass;
};

struct MetallicMaterialResources {
	AllocatedImage baseColourImage;
	VkSampler baseColourSampler;
	AllocatedImage metallicRoughnessImage;
	VkSampler metallicRoughnessSampler;
	VkBuffer dataBuffer;
	uint32_t dataBufferOffset;
};
struct MetallicMaterialConstants {
	glm::vec4 baseColorFactors;
	glm::vec4 metallicRoughnessFactors;
	//padding for uniform buffers
	glm::vec4 extra[14];
};

struct MetallicRoughnessMaterialWriter {
	VkDescriptorSetLayout materialDescriptorLayout;
	DescriptorWriter writer;
	MaterialPipelines pipelines; 

	void buildPipelines(VulkanDevice* device, VkFormat drawFormat, VkFormat depthFormat, VkDescriptorSetLayout sceneDescriptorLayout);
	void destroyPipelines(VulkanDevice* device) { pipelines.destroy(device); };
	
	MetallicMaterialInstance writeMaterialInstance(VulkanDevice* device, MaterialPassType pass, const MetallicMaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};


struct MetallicMaterialInstance : MaterialInstance {};

