#include "vk_material.h"


void MetallicRoughnessMaterialWriter::buildPipelines(VulkanDevice* device, VkFormat drawFormat, VkFormat depthFormat, VkDescriptorSetLayout sceneDescriptorLayout){
	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(DrawPushConstants);
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


MetallicMaterialInstance MetallicRoughnessMaterialWriter::writeMaterialInstance(VulkanDevice* device, MaterialPassType pass, const MetallicMaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MetallicMaterialInstance matData;
	matData.pass = pass;
	if (pass == MaterialPassType::Transparent) {
		matData.pipeline = &pipelines.transparentPipeline;
	}
	else {
		matData.pipeline = &pipelines.opaquePipeline;
	}
	

	matData.materialDescriptors = descriptorAllocator.allocate(device, materialDescriptorLayout);


	writer.clear();
	writer.writeBuffer(0, resources.dataBuffer, sizeof(MetallicMaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.writeImage(1, resources.baseColourImage.imageView, resources.baseColourSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.writeImage(2, resources.metallicRoughnessImage.imageView, resources.metallicRoughnessSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.updateSet(device->logicalDevice, matData.materialDescriptors);

	return matData;
}
