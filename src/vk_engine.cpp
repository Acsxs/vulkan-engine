
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_resources.h"
#include "vk_pipelines.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <chrono>
#include <thread>
#include <iostream>



#if NDEBUG
constexpr bool bUseValidationLayers = false;
#else
constexpr bool bUseValidationLayers = true;
#endif

using namespace std;

void FrameData::init(VulkanDevice* device) {
	
	VkCommandPoolCreateInfo commandPoolCreateInfo = vkinit::CommandPoolCreateInfo(device->getQueueFamilyIndex(VulkanDevice::GRAPHICS), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(device->logicalDevice, &commandPoolCreateInfo, nullptr, &commandPool));
	
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkinit::CommandBufferAllocateInfo(commandPool, 1);
	VK_CHECK(vkAllocateCommandBuffers(device->logicalDevice, &commandBufferAllocateInfo, &mainCommandBuffer));
	
	VkFenceCreateInfo fenceCreateInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::SemaphoreCreateInfo();
	
	VK_CHECK(vkCreateFence(device->logicalDevice, &fenceCreateInfo, nullptr, &renderFence));
	VK_CHECK(vkCreateSemaphore(device->logicalDevice, &semaphoreCreateInfo, nullptr, &swapchainSemaphore));
	VK_CHECK(vkCreateSemaphore(device->logicalDevice, &semaphoreCreateInfo, nullptr, &renderSemaphore));
}
	
void FrameData::destroy(VulkanDevice* device) {
	vkDestroySemaphore(device->logicalDevice, renderSemaphore, nullptr);
	vkDestroySemaphore(device->logicalDevice, swapchainSemaphore, nullptr);
	vkDestroyFence(device->logicalDevice, renderFence, nullptr);
	
	vkDestroyCommandPool(device->logicalDevice, commandPool, nullptr);
	
	frameDescriptors.destroyPools(device->logicalDevice);
}

void VulkanEngine::init() {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	
	window = SDL_CreateWindow(
		"Iteration 2",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		windowExtent.width,
		windowExtent.height,
		window_flags
	);
	
	SDL_SetRelativeMouseMode(SDL_TRUE);
	camera.relMouse = true;
	
	initVulkan();
	initSwapchain();
	initDescriptors();
	initPipelines();
	initDummyData();
	
	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		framesData[i].init(&vulkanDevice);
	}
	
	camera.velocity = glm::vec3(0.f);
	camera.position = glm::vec3(1, 0, 0);
	
	camera.pitch = 0;
	camera.yaw = 0;

	sceneDataBuffer.init(&vulkanDevice, sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	rendering = true;
}

void VulkanEngine::run() {
	SDL_Event e;
	bool bQuit = false;
	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0) {
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;
	
			if (e.type == SDL_WINDOWEVENT) {
	
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
					rendering = false;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
					rendering = true;
				}
			}

			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
				if (camera.relMouse == false) {
					SDL_SetRelativeMouseMode(SDL_TRUE);
					camera.relMouse = true;
				}
				else if (camera.relMouse == true) {
					SDL_SetRelativeMouseMode(SDL_FALSE);
					camera.relMouse = false;
				}
			}
			camera.processSDLEvent(e);
		}
		camera.update();

		//std::cout << "(" << camera.position.x << "," << camera.position.y << "," << camera.position.z << ")\n";
		//do not draw if we are minimized
		//std::cout << rendering << '\n';
		if (!rendering) {
			//throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		draw();
		frameCount++;
	}
}

void VulkanEngine::draw() {

	VkClearColorValue clearColour = { .float32 = {0.0, 0.0, 0.0} };
	FrameData* currentFrame = &getCurrentFrameData();
	VK_CHECK(vkWaitForFences(vulkanDevice.logicalDevice, 1, &currentFrame->renderFence, true, 1000000000));
	currentFrame->frameDescriptors.clearPools(vulkanDevice.logicalDevice);
	
	camera.update();

	glm::mat4 view = camera.getViewMatrix();

	// camera projection
	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)windowExtent.width / (float)windowExtent.height, 10000.f, 0.1f);


	projection[1][1] *= -1;

	sceneData.view = view;
	sceneData.proj = projection;
	sceneData.viewproj = projection * view;
	sceneData.viewPos = glm::vec4(camera.position, 0);

	//some default lighting parameters
	sceneData.ambientColor = glm::vec4(.2f, .2f, .2f, 1.f);
	sceneData.sunlightColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	sceneData.sunlightDirection = glm::vec4(0, -1, -0.5, 10.f);

	
	uint32_t swapchainImageIndex;
	VkResult e = vkAcquireNextImageKHR(vulkanDevice.logicalDevice, vulkanSwapchain.swapchain, 1000000000, currentFrame->swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		SDL_GetWindowSizeInPixels(window, (int*)&windowExtent.width, (int*)&windowExtent.height);
		vulkanSwapchain.rebuildSwapchain(&vulkanDevice, surface, windowExtent.height, windowExtent.width);
		return;
	}


	
	VK_CHECK(vkResetFences(vulkanDevice.logicalDevice, 1, &currentFrame->renderFence));
	
	VK_CHECK(vkResetCommandBuffer(currentFrame->mainCommandBuffer, 0));
	
	VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	
	drawExtent.width = drawImage.imageExtent.width;
	drawExtent.height = drawImage.imageExtent.height;


	VK_CHECK(vkBeginCommandBuffer(currentFrame->mainCommandBuffer, &commandBufferBeginInfo));

	drawImage.transitionImage(&currentFrame->mainCommandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkImageSubresourceRange imageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCmdClearColorImage(currentFrame->mainCommandBuffer, drawImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColour, 1, &imageSubresourceRange);


	drawImage.transitionImage(&currentFrame->mainCommandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	depthImage.transitionImage(&currentFrame->mainCommandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
	
	drawGeometry(&currentFrame->mainCommandBuffer, currentFrame);
	
	drawImage.transitionImage(&currentFrame->mainCommandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	
	vulkanSwapchain.transitionSwapchainImage(&currentFrame->mainCommandBuffer, swapchainImageIndex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	drawImage.copyToImage(&currentFrame->mainCommandBuffer, &vulkanSwapchain.swapchainImages[swapchainImageIndex], vulkanSwapchain.swapchainExtent);
	
	vulkanSwapchain.transitionSwapchainImage(&currentFrame->mainCommandBuffer, swapchainImageIndex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	
	VK_CHECK(vkEndCommandBuffer(currentFrame->mainCommandBuffer));




	VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::CommandBufferSubmitInfo(currentFrame->mainCommandBuffer);
	
	VkSemaphoreSubmitInfo waitInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, currentFrame->swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currentFrame->renderSemaphore);
	
	VkSubmitInfo2 submitInfo = vkinit::SubmitInfo2(&commandBufferSubmitInfo, &signalInfo, &waitInfo);
	
	VK_CHECK(vkQueueSubmit2(*vulkanDevice.queues[VulkanDevice::GRAPHICS].get(), 1, &submitInfo, currentFrame->renderFence));
	
	VkPresentInfoKHR presentInfo = vkinit::PresentInfoKHR();
	
	presentInfo.pSwapchains = &vulkanSwapchain.swapchain;
	presentInfo.swapchainCount = 1;
	
	presentInfo.pWaitSemaphores = &currentFrame->renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;
	
	presentInfo.pImageIndices = &swapchainImageIndex;
	
	VkResult presentResult = vkQueuePresentKHR(*vulkanDevice.queues[VulkanDevice::GRAPHICS].get(), &presentInfo);
}

void VulkanEngine::destroy() {
	for (FrameData frame : framesData) { frame.destroy(&vulkanDevice); }

	vkDestroyDescriptorSetLayout(vulkanDevice.logicalDevice, sceneDataDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkanDevice.logicalDevice, drawImageDescriptorLayout, nullptr);
	vkDestroyDescriptorPool(vulkanDevice.logicalDevice, imguiDescriptorPool, nullptr);
	globalDescriptorAllocator.destroyPools(vulkanDevice.logicalDevice);

	drawImage.destroy(&vulkanDevice);
	depthImage.destroy(&vulkanDevice);
	defaultPipeline.destroy(&vulkanDevice);
	sceneDataBuffer.destroy(&vulkanDevice);
	vertexBuffer.destroy(&vulkanDevice);
	indexBuffer.destroy(&vulkanDevice);

	vulkanSwapchain.destroySwapchain(&vulkanDevice);
	vulkanDevice.destroy();
	vkb::destroy_debug_utils_messenger(instance, debugMessenger, nullptr);
	vkDestroyInstance(instance, nullptr);
}

void VulkanEngine::drawGeometry(VkCommandBuffer* commandBuffer, FrameData* frame) {

	//write the buffer
	SceneData* sceneUniformData = (SceneData*)sceneDataBuffer.allocation->GetMappedData();
	*sceneUniformData = sceneData;



	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = frame->frameDescriptors.allocate(vulkanDevice.logicalDevice, sceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.writeBuffer(0, sceneDataBuffer.buffer, sizeof(SceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); // something makes bind 0 a sampler, find it
	writer.updateSet(vulkanDevice.logicalDevice, globalDescriptor);

	VkRenderingAttachmentInfo colorAttachment = vkinit::RenderingAttachmentInfo(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = vkinit::DepthAttachmentInfo(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);


	VkRenderingInfo renderInfo = vkinit::RenderingInfo(drawExtent, &colorAttachment, &depthAttachment);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = drawExtent.width;
	viewport.height = drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;


	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = drawExtent.width;
	scissor.extent.height = drawExtent.height;

	vkCmdBeginRendering(*commandBuffer, &renderInfo);

	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, defaultPipeline.pipeline);
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, defaultPipeline.pipelineLayout, 0, 1, &globalDescriptor, 0, nullptr);

	vkCmdSetViewport(*commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(*commandBuffer, 0, 1, &scissor);


	vkCmdBindIndexBuffer(*commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	GlobalGeometryPushConstants pushConstants = {};
	pushConstants.vertexBuffer = vertexBufferAddress;
	pushConstants.worldMatrix = glm::mat4{1.f};
	vkCmdPushConstants(*commandBuffer, defaultPipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GlobalGeometryPushConstants), &pushConstants);

	vkCmdDrawIndexed(*commandBuffer, indices.size(), 1, 0, 0, 0);
	//vkCmdDraw(*commandBuffer, 3, 1, 0, 0);


	vkCmdEndRendering(*commandBuffer);

}


void VulkanEngine::initVulkan() {
	vkb::InstanceBuilder vkbInstanceBuilder;

	vkb::Instance vkbInstance = vkbInstanceBuilder
		.set_app_name("Iteration 2")
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build()
		.value();

	instance = vkbInstance.instance;
	debugMessenger = vkbInstance.debug_messenger;

	SDL_Vulkan_CreateSurface(window, instance, &surface);

	VkPhysicalDeviceVulkan13Features physicalDeviceFeatures13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	physicalDeviceFeatures13.dynamicRendering = true;
	physicalDeviceFeatures13.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features physicalDeviceFeatures12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	physicalDeviceFeatures12.bufferDeviceAddress = true;
	physicalDeviceFeatures12.descriptorIndexing = true;

	vkb::PhysicalDeviceSelector vkbPhysicalDeviceSelector{ vkbInstance };
	vkb::PhysicalDevice vkbPhysicalDevice = vkbPhysicalDeviceSelector
		.set_minimum_version(1, 3)
		.set_required_features_13(physicalDeviceFeatures13)
		.set_required_features_12(physicalDeviceFeatures12)
		.set_surface(surface)
		.select()
		.value();

	vulkanDevice.init(vkbPhysicalDevice, instance);
}

void VulkanEngine::initSwapchain() {
	vulkanSwapchain.createSwapchain(&vulkanDevice, surface, windowExtent.width, windowExtent.height);
	VkExtent3D drawImageExtent = {
		windowExtent.width,
		windowExtent.height,
		1
	};
		
	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		
	VkImageCreateInfo renderImageCreateInfo = vkinit::ImageCreateInfo(drawImage.imageFormat, drawImageUsages, drawImageExtent);
		
	VmaAllocationCreateInfo renderImageAllocationCreateInfo = {};
	renderImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	renderImageAllocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	drawImage.init(&vulkanDevice, drawImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages, VK_IMAGE_ASPECT_COLOR_BIT);
		
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		
	depthImage.init(&vulkanDevice, drawImageExtent, VK_FORMAT_D32_SFLOAT, depthImageUsages, VK_IMAGE_ASPECT_DEPTH_BIT);

}

void VulkanEngine::initPipelines() {

	VkShaderModule fragmentShader;
	VkShaderModule vertexShader;

	if (vkutil::loadShaderModule("../../shaders/draw.frag.spv", vulkanDevice.logicalDevice, &fragmentShader)) {
		fmt::println("Fragment shader module loaded successfully");
	}
	else {
		fmt::println("Error when building the mesh fragment shader module");
	}
	if (vkutil::loadShaderModule("../../shaders/draw.vert.spv", vulkanDevice.logicalDevice, &vertexShader)) {
		fmt::println("Vertex shader module loaded successfully");
	}
	else {
		fmt::println("Error when building the mesh vertex shader module");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GlobalGeometryPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::PipelineLayoutCreateInfo();
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &sceneDataDescriptorLayout;
	pipelineLayoutInfo.pPushConstantRanges = &matrixRange;
	pipelineLayoutInfo.pushConstantRangeCount = 1;

	PipelineInfo defaultPipelineInfo = {};
	defaultPipelineInfo.vertexShader = vertexShader;
	defaultPipelineInfo.fragmentShader = fragmentShader;
	defaultPipelineInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	defaultPipelineInfo.mode = VK_POLYGON_MODE_FILL;
	defaultPipelineInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	defaultPipelineInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	defaultPipelineInfo.blending = PipelineInfo::NONE;
	defaultPipelineInfo.colourAttachmentFormat = drawImage.imageFormat;
	defaultPipelineInfo.depthFormat = depthImage.imageFormat;
	defaultPipelineInfo.depthWriteEnable = true;
	defaultPipelineInfo.depthCompareOperation = VK_COMPARE_OP_GREATER_OR_EQUAL;
	defaultPipelineInfo.pipelineLayoutInfo = pipelineLayoutInfo;
	
	defaultPipeline.init(&vulkanDevice, defaultPipelineInfo);

	vkDestroyShaderModule(vulkanDevice.logicalDevice, fragmentShader, nullptr);
	vkDestroyShaderModule(vulkanDevice.logicalDevice, vertexShader, nullptr);

}

void VulkanEngine::initDescriptors() {

	std::vector < DescriptorAllocatorGrowable::PoolSizeRatio > sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
	};

	globalDescriptorAllocator.init(vulkanDevice.logicalDevice, 10, sizes);

	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		sceneDataDescriptorLayout = builder.build(vulkanDevice.logicalDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

}

void VulkanEngine::initDummyData() {

	vertices = {
		Vertex { glm::vec4(-1.0, -1.0, -1.0, 1.0), glm::vec4(1.0, 1.0, 1.0, 1.0)}, //0
		Vertex { glm::vec4(1.0, -1.0, -1.0, 1.0), glm::vec4(0.0, 0.0, 1.0, 1.0)}, //1
		Vertex { glm::vec4(1.0,  1.0, -1.0, 1.0), glm::vec4(0.0, 1.0, 0.0, 1.0)}, //2
		Vertex { glm::vec4(-1.0,  1.0, -1.0, 1.0), glm::vec4(1.0, 0.0, 0.0, 1.0)}, //3

		Vertex { glm::vec4(-1.0, -1.0,  1.0, 1.0), glm::vec4(1.0, 1.0, 0.0, 1.0)}, //4
		Vertex { glm::vec4(1.0, -1.0,  1.0, 1.0), glm::vec4(1.0, 0.0, 1.0, 1.0)}, //5
		Vertex { glm::vec4(1.0,  1.0,  1.0, 1.0), glm::vec4(0.0, 1.0, 1.0, 1.0)}, //6
		Vertex { glm::vec4(-1.0,  1.0,  1.0, 1.0), glm::vec4(0.5, 0.0, 1.0, 1.0)}, //7
	};
	indices = {
			 0, 3, 2, 2, 1, 0,
			 4, 5, 6, 6, 7, 4,
			 7, 6, 2, 2, 3, 7,
			 5, 1, 2, 2, 6, 5,
			 4, 0, 1, 1, 5, 4,
			 0, 4, 7, 7, 3, 0
	};

	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	vertexBuffer.init(&vulkanDevice, vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = vertexBuffer.buffer };
	vertexBufferAddress = vkGetBufferDeviceAddress(vulkanDevice.logicalDevice, &deviceAdressInfo);

	indexBuffer.init(&vulkanDevice, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	AllocatedBuffer staging = {};
	staging.init(&vulkanDevice, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = staging.allocation->GetMappedData();
	// copy vertex buffer
	memcpy(data, vertices.data(), vertexBufferSize);
	// copy index buffer
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	vulkanDevice.immediateSubmit([&](VkCommandBuffer* commandBuffer) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(*commandBuffer, staging.buffer, vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(*commandBuffer, staging.buffer, indexBuffer.buffer, 1, &indexCopy);
		});

	staging.destroy(&vulkanDevice);
}