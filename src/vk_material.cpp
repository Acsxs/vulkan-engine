#include "vk_material.h"


void SpecularMaterialWriter::buildPipelines(VulkanDevice* device, VkFormat drawFormat, VkFormat depthFormat, VkDescriptorSetLayout sceneDescriptorLayout){
	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); 

	materialDescriptorLayout = layoutBuilder.build(device->logicalDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { sceneDescriptorLayout, materialDescriptorLayout };

	VkPipelineLayoutCreateInfo materialPipelineLayoutInfo = vkinit::PipelineLayoutCreateInfo();
	materialPipelineLayoutInfo.setLayoutCount = 2;
	materialPipelineLayoutInfo.pSetLayouts = layouts;
	materialPipelineLayoutInfo.pPushConstantRanges = &matrixRange;
	materialPipelineLayoutInfo.pushConstantRangeCount = 1;

	pipelines.init(device, drawFormat, depthFormat, materialPipelineLayoutInfo);

}


SpecularMaterialReference SpecularMaterialWriter::writeMaterialInstance(VulkanDevice* device, MaterialPassType pass, const SpecularMaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	SpecularMaterialReference matData;
	matData.pass = pass;
	if (pass == MaterialPassType::Transparent) {
		matData.pipeline = &pipelines.transparentPipeline;
	}
	else {
		matData.pipeline = &pipelines.opaquePipeline;
	}
	

	matData.materialDescriptors = descriptorAllocator.allocate(device->logicalDevice, materialDescriptorLayout);


	writer.clear();
	writer.writeBuffer(0, resources.dataBuffer, sizeof(SpecularMaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.writeImage(1, resources.albedoImage.imageView, resources.albedoSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.writeImage(2, resources.specularGlossinessImage.imageView, resources.specularGlossinessSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.updateSet(device->logicalDevice, matData.materialDescriptors);

	return matData;
}
