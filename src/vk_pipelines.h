#pragma once 
#include <vk_types.h>

namespace vkutil {

	bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
};

class PipelineBuilder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;
    VkFormat _colorAttachmentformat;

    PipelineBuilder() { clear(); }

    void clear();

    VkPipeline buildPipeline(VkDevice device);

    void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    void setInputTopology(VkPrimitiveTopology topology);

    void setPolygonMode(VkPolygonMode mode);

    void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

    void setMultisamplingNone();

    void disableBlending();

    void setColorAttachmentFormat(VkFormat format);

    void setDepthFormat(VkFormat format);
    
    void disableDepthtest();

    void enableDepthtest(bool depthWriteEnable, VkCompareOp op);
};
