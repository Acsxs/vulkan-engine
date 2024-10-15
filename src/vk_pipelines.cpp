﻿#include "vk_pipelines.h"
#include <fstream>
#include "vk_initializers.h"

bool vkutil::loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
{
    // open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read((char*)buffer.data(), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    shaderModuleCreateInfo.codeSize = buffer.size() * sizeof(uint32_t);
    shaderModuleCreateInfo.pCode = buffer.data();

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return false;
    }
    *outShaderModule = shaderModule;
    return true;
}

VkPipeline vkutil::buildPipeline(VulkanDevice* device, PipelineInfo info) {

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(vkinit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, info.vertexShader));
    shaderStages.push_back(vkinit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, info.fragmentShader));


    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology = info.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.polygonMode = info.mode;
    rasterizer.lineWidth = 1.f;
    rasterizer.cullMode = info.cullMode;
    rasterizer.frontFace = info.frontFace;

    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (info.blending == PipelineInfo::NONE) {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }
    else if (info.blending == PipelineInfo::ADDITIVE) {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    else if (info.blending == PipelineInfo::ALPHA) {
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineMultisampleStateCreateInfo multisampling;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = info.depthWriteEnable;
    depthStencil.depthCompareOp = info.depthCompareOperation;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};
    depthStencil.minDepthBounds = 0.f;
    depthStencil.maxDepthBounds = 1.f;

    VkPipelineRenderingCreateInfo renderInfo;
    renderInfo.depthAttachmentFormat = info.depthFormat;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &info.colourAttachmentFormat;






    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };


    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    // connect the renderInfo to the pNext extension mechanism
    graphicsPipelineCreateInfo.pNext = &renderInfo;

    graphicsPipelineCreateInfo.stageCount = (uint32_t)shaderStages.size();
    graphicsPipelineCreateInfo.pStages = shaderStages.data();
    graphicsPipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    graphicsPipelineCreateInfo.pViewportState = &viewportState;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizer;
    graphicsPipelineCreateInfo.pMultisampleState = &multisampling;
    graphicsPipelineCreateInfo.pColorBlendState = &colorBlending;
    graphicsPipelineCreateInfo.pDepthStencilState = &depthStencil;
    graphicsPipelineCreateInfo.layout = info.pipelineLayout;
    graphicsPipelineCreateInfo.pDynamicState = &dynamicInfo;

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device->_logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        fmt::println("failed to create pipeline");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    }
    else {
        return newPipeline;
    }
};

void VulkanPipeline::init(VulkanDevice* device, PipelineInfo info) {
    pipelineLayout = info.layouts;
    pipeline = vkutil::buildPipeline(device, info);
}


void MaterialPipeline::init(VulkanDevice* device, VkFormat drawFormat, VkFormat depthFormat, VkDescriptorSetLayout* layouts, VkPushConstantRange* pushConstantRanges) {
    VkShaderModule meshFragShader;
    if (vkutil::loadShaderModule("../../shaders/mesh.frag.spv", device->_logicalDevice, &meshFragShader)) {
        fmt::println("Mesh fragment shader module loaded successfully");
    }
    else {
        fmt::println("Error when building the mesh fragment shader module");
    }
    VkShaderModule meshVertexShader;
    if (vkutil::loadShaderModule("../../shaders/mesh.vert.spv", device->_logicalDevice, &meshVertexShader)) {
        fmt::println("Mesh vertex shader module loaded successfully");
    }
    else {
        fmt::println("Error when building the mesh vertex shader module");
    }

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::PipelineLayoutCreateInfo();
    mesh_layout_info.setLayoutCount = 2;
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = pushConstantRanges;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(device->_logicalDevice, &mesh_layout_info, nullptr, &newLayout));


    PipelineInfo pipelineInfo = {};

    pipelineInfo.vertexShader = meshVertexShader;
    pipelineInfo.fragmentShader = meshFragShader;
    pipelineInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineInfo.mode = VK_POLYGON_MODE_FILL;
    pipelineInfo.cullMode = VK_CULL_MODE_NONE;
    pipelineInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    pipelineInfo.blending = PipelineInfo::NONE;
    pipelineInfo.colourAttachmentFormat = drawFormat;
    pipelineInfo.depthFormat = depthFormat;
    pipelineInfo.depthWriteEnable = true;
    pipelineInfo.depthCompareOperation = VK_COMPARE_OP_GREATER_OR_EQUAL;
    pipelineInfo.pipelineLayout = newLayout;

    opaquePipeline.init(device, pipelineInfo);

    pipelineInfo.blending = PipelineInfo::ALPHA;
    transparentPipeline.init(device, pipelineInfo);

    vkDestroyShaderModule(device->_logicalDevice, meshFragShader, nullptr);
    vkDestroyShaderModule(device->_logicalDevice, meshVertexShader, nullptr);
}