#include "vk_pipelines.h"
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

VkPipeline vkutil::buildPipeline(VulkanDevice* device, VkPipelineLayout layout, PipelineInfo info) {

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(vkinit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, info.vertexShader));
    shaderStages.push_back(vkinit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, info.fragmentShader));


    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = info.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.polygonMode = info.mode;
    rasterizer.lineWidth = 1.f;
    rasterizer.cullMode = info.cullMode;
    rasterizer.frontFace = info.frontFace;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (info.blending == PipelineInfo::NONE) {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }
    else if (info.blending == PipelineInfo::ADDITIVE) {
        // destination is current colour + (new colour* new color alpha)
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }  else if (info.blending == PipelineInfo::ALPHA) {
        // destination is (current colour*(1-new color alpha)) + (new colour* new color alpha)
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    // No multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    if (info.doDepthTest) {
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = info.depthWriteEnable;
        depthStencil.depthCompareOp = info.depthCompareOperation;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};
        depthStencil.minDepthBounds = 0.f;
        depthStencil.maxDepthBounds = 1.f;
    }  else {
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};
        depthStencil.minDepthBounds = 0.f;
        depthStencil.maxDepthBounds = 1.f;
    }

    VkPipelineRenderingCreateInfo renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &info.colourAttachmentFormat;
    renderInfo.depthAttachmentFormat = info.depthFormat;



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
    graphicsPipelineCreateInfo.layout = layout;
    graphicsPipelineCreateInfo.pDynamicState = &dynamicInfo;

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        fmt::println("failed to create pipeline");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    }
    else {
        return newPipeline;
    }
};
void VulkanPipeline::init(VulkanDevice* device, PipelineInfo info) {
    VK_CHECK(vkCreatePipelineLayout(device->logicalDevice, &info.pipelineLayoutInfo, nullptr, &pipelineLayout));
    pipeline = vkutil::buildPipeline(device, pipelineLayout, info);
}


void MaterialPipelines::init(VulkanDevice* device, VkFormat drawFormat, VkFormat depthFormat, VkPipelineLayoutCreateInfo layout) {
    VkShaderModule meshFragShader;
    if (vkutil::loadShaderModule("../../shaders/mesh.frag.spv", device->logicalDevice, &meshFragShader)) {
        fmt::println("Mesh fragment shader module loaded successfully");
    }
    else {
        fmt::println("Error when building the mesh fragment shader module");
    }
    VkShaderModule meshVertexShader;
    if (vkutil::loadShaderModule("../../shaders/mesh.vert.spv", device->logicalDevice, &meshVertexShader)) {
        fmt::println("Mesh vertex shader module loaded successfully");
    }
    else {
        fmt::println("Error when building the mesh vertex shader module");
    }

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
    pipelineInfo.pipelineLayoutInfo = layout;

    opaquePipeline.init(device, pipelineInfo);
   
    pipelineInfo.blending = PipelineInfo::ALPHA;
    transparentPipeline.init(device, pipelineInfo);

    vkDestroyShaderModule(device->logicalDevice, meshFragShader, nullptr);
    vkDestroyShaderModule(device->logicalDevice, meshVertexShader, nullptr);
}