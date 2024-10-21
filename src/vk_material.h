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


struct MaterialReference {
	VulkanPipeline* pipeline;
	VkDescriptorSet materialDescriptors;
	MaterialPassType pass;
};

struct SpecularMaterialResources {
	AllocatedImage albedoImage;
	VkSampler albedoSampler;
	AllocatedImage specularGlossinessImage;
	VkSampler specularGlossinessSampler;
	VkBuffer dataBuffer;
	uint32_t dataBufferOffset;
};
struct SpecularMaterialConstants {
	glm::vec4 albedoFactors;
	glm::vec4 specularGlossinessFactors;
	//padding for uniform buffers
	glm::vec4 extra[14];
};

struct SpecularMaterialWriter {
	VkDescriptorSetLayout materialDescriptorLayout;
	DescriptorWriter writer;
	MaterialPipelines pipelines; 

	void buildPipelines(VulkanDevice* device, VkFormat drawFormat, VkFormat depthFormat, VkDescriptorSetLayout sceneDescriptorLayout);
	void destroyPipelines(VulkanDevice* device) { pipelines.destroy(device); };
	
	SpecularMaterialReference writeMaterialInstance(VulkanDevice* device, MaterialPassType pass, const SpecularMaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};


struct SpecularMaterialReference : MaterialReference {};

