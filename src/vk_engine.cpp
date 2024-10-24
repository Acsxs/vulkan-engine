
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_resources.h"
#include "vk_pipelines.h"

#include <glm/gtx/transform.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <VkBootstrap.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"


#include <chrono>
#include <thread>



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
	frameDescriptors.destroyPools(device);
	vkDestroySemaphore(device->logicalDevice, renderSemaphore, nullptr);
	vkDestroySemaphore(device->logicalDevice, swapchainSemaphore, nullptr);
	vkDestroyFence(device->logicalDevice, renderFence, nullptr);

	vkDestroyCommandPool(device->logicalDevice, commandPool, nullptr);
}




void VulkanEngine::init() {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	
	window = SDL_CreateWindow(
		"Vulkan Engine",
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
	initImgui();
	initData();
	
	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		framesData[i].init(&vulkanDevice);
	}
	
	camera.velocity = glm::vec3(0.f);
	camera.position = glm::vec3(0, 0, 5);
	
	camera.pitch = 0;
	camera.yaw = 0;

	sceneDataBuffer.init(&vulkanDevice, sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	vmaMapMemory(vulkanDevice.allocator, sceneDataBuffer.allocation, &sceneDataAllocation);

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
			//send SDL event to imgui for handling
			ImGui_ImplSDL2_ProcessEvent(&e);
		}
		camera.update();
	
		//do not draw if we are minimized
		if (!rendering) {
			//throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
	
		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
	
		// imgui input sensitivity
		ImGui::NewFrame();
	
		if (ImGui::Begin("input sensitivity")) {
	
	
			ImGui::SliderFloat("movespeed", &camera.moveSpeed, 0, 2);
			ImGui::SliderFloat("mouse sense", &camera.mouseSense, 0, 2);
	
			ImGui::End();
		}
		ImGui::Render();
	
		draw();
	}
}

void VulkanEngine::draw() {

	VkClearColorValue clearColour = { .float32 = {0.0, 0.0, 0.0} };
	FrameData* currentFrame = &getCurrentFrameData();
	VK_CHECK(vkWaitForFences(vulkanDevice.logicalDevice, 1, &currentFrame->renderFence, true, 1000000000));
	currentFrame->frameDescriptors.clearPools(&vulkanDevice);

	appendSceneDraws();

	camera.update();

	glm::mat4 view = camera.getViewMatrix();

	// camera projection
	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)windowExtent.width / (float)windowExtent.height, 10000.f, 0.1f);


	projection[1][1] *= -1;

	sceneData.view = view;
	sceneData.projection = projection;
	sceneData.viewProjection = projection * view;
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

void VulkanEngine::appendSceneDraws() {
	drawObjectCollection.opaqueObjects.clear();
	drawObjectCollection.transparentObjects.clear();

	glm::mat4 topMatrix = glm::mat4{ 1.f };
	for (std::shared_ptr<VulkanGLTFModel> model : models) {
		model->addNodeDraws(topMatrix, &drawObjectCollection);
	}	
}

void VulkanEngine::drawGeometry(VkCommandBuffer* commandBuffer, FrameData* frame) {

	memcpy(sceneDataAllocation, &sceneData, sizeof(SceneData));

	VkDescriptorSet sceneDataDescriptor = frame->frameDescriptors.allocate(&vulkanDevice, sceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.writeBuffer(0, sceneDataBuffer.buffer, sizeof(SceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); // something makes bind 0 a sampler, find it
	writer.updateSet(vulkanDevice.logicalDevice, sceneDataDescriptor);

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
	std::cout << drawObjectCollection.opaqueObjects.size() << " " << drawObjectCollection.transparentObjects.size() << "\n";
	auto draw = [&](DrawObjectInfo* draw) {

		std::cout << (draw->material.get()->pipeline->pipeline) << "\n";
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->material->pipeline->pipeline);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->material->pipeline->pipelineLayout, 0, 1, &sceneDataDescriptor, 0, nullptr);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->material->pipeline->pipelineLayout, 1, 1, &draw->material->materialDescriptors, 0, nullptr);

		vkCmdSetViewport(*commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(*commandBuffer, 0, 1, &scissor);

		vkCmdBindIndexBuffer(*commandBuffer, draw->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		DrawPushConstants pushConstants;
		pushConstants.vertexBuffer = draw->vertexBufferAddress;
		pushConstants.worldMatrix = draw->transform;
		vkCmdPushConstants(*commandBuffer, draw->material->pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DrawPushConstants), &pushConstants);

		vkCmdDrawIndexed(*commandBuffer, draw->indexCount, 1, draw->firstIndex, 0, 0);
		};

	for (int i = 0; i < drawObjectCollection.opaqueObjects.size(); i++) {
		draw(drawObjectCollection.opaqueObjects[i]);
	}

	for (int i = 0; i < drawObjectCollection.transparentObjects.size(); i++) {
		draw(drawObjectCollection.transparentObjects[i]);
	}


	vkCmdEndRendering(*commandBuffer);
}

void VulkanEngine::destroy() {
	vkDeviceWaitIdle(vulkanDevice.logicalDevice);

	for (std::shared_ptr<VulkanGLTFModel> model : models) { model.get()->destroy(&vulkanDevice); }
	models.clear();
	for (FrameData frame : framesData) { frame.destroy(&vulkanDevice); }

	drawImage.destroy(&vulkanDevice);
	depthImage.destroy(&vulkanDevice);
	vmaUnmapMemory(vulkanDevice.allocator, sceneDataBuffer.allocation);
	sceneDataBuffer.destroy(&vulkanDevice);
	globalDescriptorAllocator.destroyPools(&vulkanDevice);

	vkDestroyDescriptorSetLayout(vulkanDevice.logicalDevice, sceneDataDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkanDevice.logicalDevice, drawImageDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(vulkanDevice.logicalDevice, materialWriter.materialDescriptorLayout, nullptr);
	vkDestroyDescriptorPool(vulkanDevice.logicalDevice,imguiDescriptorPool, nullptr);

	vulkanSwapchain.destroySwapchain(&vulkanDevice);
	vulkanDevice.destroy();
	vkb::destroy_debug_utils_messenger(instance, debugMessenger, nullptr);
	vkDestroyInstance(instance, nullptr);
}



void VulkanEngine::initVulkan() {
	vkb::InstanceBuilder vkbInstanceBuilder;

	vkb::Instance vkbInstance = vkbInstanceBuilder
		.set_app_name("Refactor")
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

void VulkanEngine::initImgui() {
	//the size of the pool is very oversize, but it's copied from imgui demo itself.
	VkDescriptorPoolSize descriptorPoolSizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptorPoolCreateInfo.maxSets = 1000;
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)std::size(descriptorPoolSizes);
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;

	VK_CHECK(vkCreateDescriptorPool(vulkanDevice.logicalDevice, &descriptorPoolCreateInfo, nullptr, &imguiDescriptorPool));

	ImGui::CreateContext();

	ImGui_ImplSDL2_InitForVulkan(window);

	ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
	imguiInitInfo.Instance = instance;
	imguiInitInfo.PhysicalDevice = vulkanDevice.physicalDevice;
	imguiInitInfo.Device = vulkanDevice.logicalDevice;
	imguiInitInfo.Queue = *vulkanDevice.queues[VulkanDevice::GRAPHICS].get();
	imguiInitInfo.DescriptorPool = imguiDescriptorPool;
	imguiInitInfo.MinImageCount = 3;
	imguiInitInfo.ImageCount = 3;
	imguiInitInfo.UseDynamicRendering = true;

	imguiInitInfo.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	imguiInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	imguiInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &vulkanSwapchain.swapchainImageFormat;


	imguiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&imguiInitInfo);

	ImGui_ImplVulkan_CreateFontsTexture();
}

void VulkanEngine::initPipelines() {

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);


	VkDescriptorSetLayout materialLayout = layoutBuilder.build(vulkanDevice.logicalDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { sceneDataDescriptorLayout, materialLayout };

	materialWriter.buildPipelines(&vulkanDevice, drawImage.imageFormat, depthImage.imageFormat, sceneDataDescriptorLayout);
}

void VulkanEngine::initDescriptors() {
	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
	};

	globalDescriptorAllocator.init(&vulkanDevice, 10, sizes);


	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		sceneDataDescriptorLayout = builder.build(vulkanDevice.logicalDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		// support for compute pipelines
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		drawImageDescriptorLayout = builder.build(vulkanDevice.logicalDevice, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	drawImageDescriptors = globalDescriptorAllocator.allocate(&vulkanDevice, drawImageDescriptorLayout);

	{
		DescriptorWriter writer;
		writer.writeImage(0, drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

		writer.updateSet(vulkanDevice.logicalDevice, drawImageDescriptors);
	}


	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		framesData[i].frameDescriptors = DescriptorAllocatorGrowable{};
		framesData[i].frameDescriptors.init(&vulkanDevice, 1000, frame_sizes);

	}
}

void VulkanEngine::initData() {
	VulkanGLTFModel model;
	model.init(&vulkanDevice, "../../assets/structure.glb", &materialWriter);
	models.push_back(std::make_shared<VulkanGLTFModel>(model));

}