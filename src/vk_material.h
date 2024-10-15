#pragma once

#include "vk_types.h"
#include "vk_resources.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_device.h"


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


struct SpecularMaterial {
	VkDescriptorSetLayout materialDescriptorLayout;
	DescriptorWriter writer;
	MaterialPipeline pipelines;

	void buildPipelines(VulkanDevice* device, VkDescriptorSetLayout sceneDescriptorLayout);
	void destroyPipelines(VulkanDevice* device) { pipelines.destroy(device); };
	
	MaterialInstance writeMaterialReference(VulkanDevice* device, PassType pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct SpecularMaterialReference {};

//struct SpecularMaterial {
//
//	VulkanPipeline opaquePipeline;
//	VulkanPipeline transparentPipeline;
//
//	struct MaterialResources {
//		AllocatedImage albedoImage;
//		VkSampler albedoSampler;
//		AllocatedImage specularGlossinessImage;
//		VkSampler specularGlossinessSampler;
//		VkBuffer dataBuffer;
//		uint32_t dataBufferOffset;
//	};
//	struct MaterialConstants {
//		glm::vec4 albedoFactors;
//		glm::vec4 specularGlossinessFactors;
//		//padding, we need it anyway for uniform buffers
//		glm::vec4 extra[14];
//	};
//
//	void buildPipelines(VulkanEngine* engine);
//	void destroyPipelines(VulkanDevice* device);
//
//	void clearResources(VulkanDevice* device);
//	MaterialInstance writeMaterialReference(VulkanDevice* device, PassType pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
//};