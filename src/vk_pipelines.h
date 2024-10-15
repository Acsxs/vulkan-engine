#pragma once 
#include "vk_types.h"
#include "vk_device.h"

struct PipelineInfo {
    enum BlendState {
        NONE,
        ADDITIVE,
        ALPHA
    };
    VkShaderModule vertexShader;
    VkShaderModule fragmentShader;
    VkPrimitiveTopology topology;
    VkPolygonMode mode;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    BlendState blending;
    VkFormat colourAttachmentFormat;
    VkFormat depthFormat;
    bool depthWriteEnable;
    VkCompareOp depthCompareOperation;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout* layouts;
    VkPushConstantRange* pushConstantRanges
};

namespace vkutil {

	bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
    VkPipeline buildPipeline(VulkanDevice* device, PipelineInfo info);
};

struct VulkanPipeline {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    void init(VulkanDevice* device, PipelineInfo info);
    void destroy(VulkanDevice* device) { vkDestroyPipeline(device->_logicalDevice, pipeline, nullptr); vkDestroyPipelineLayout(device->_logicalDevice, pipelineLayout, nullptr); };
};


struct MaterialPipeline {
    VulkanPipeline opaquePipeline;
    VulkanPipeline transparentPipeline;
    void init (VulkanDevice* device, VkFormat drawFormat, VkFormat depthFormat, VkDescriptorSetLayout* layouts, VkPushConstantRange* pushConstantRanges);
    void destroy(VulkanDevice* device) { opaquePipeline.destroy(device); transparentPipeline.destroy(device); };
};