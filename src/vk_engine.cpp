
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

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <chrono>
#include <thread>



#if NDEBUG
constexpr bool bUseValidationLayers = false;
#else
constexpr bool bUseValidationLayers = true;
#endif

using namespace std;


void RenderData::init(VulkanDevice* device) {

	VkCommandPoolCreateInfo commandPoolCreateInfo = vkinit::CommandPoolCreateInfo(device->getQueueFamilyIndex(VulkanDevice::GRAPHICS), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(device->_logicalDevice, &commandPoolCreateInfo, nullptr, &_commandPool));

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkinit::CommandBufferAllocateInfo(_commandPool, 1);
	VK_CHECK(vkAllocateCommandBuffers(device->_logicalDevice, &commandBufferAllocateInfo, &_mainCommandBuffer));

	VkFenceCreateInfo fenceCreateInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::SemaphoreCreateInfo();

	VK_CHECK(vkCreateFence(device->_logicalDevice, &fenceCreateInfo, nullptr, &_renderFence));
	VK_CHECK(vkCreateSemaphore(device->_logicalDevice, &semaphoreCreateInfo, nullptr, &_swapchainSemaphore));
	VK_CHECK(vkCreateSemaphore(device->_logicalDevice, &semaphoreCreateInfo, nullptr, &_renderSemaphore));
}

void RenderData::destroy(VulkanDevice* device) {
	vkDestroySemaphore(device->_logicalDevice, _renderSemaphore, nullptr);
	vkDestroySemaphore(device->_logicalDevice, _swapchainSemaphore, nullptr);
	vkDestroyFence(device->_logicalDevice, _renderFence, nullptr);

	vkDestroyCommandPool(device->_logicalDevice, _commandPool, nullptr);

	_frameDescriptors.destroyPools(device->_logicalDevice);
}


void VulkanEngine::init()
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);

	SDL_SetRelativeMouseMode(SDL_TRUE);
	mainCamera.relMouse = true;

	initVulkan();
	initSwapchain();
	initCommands();
	initSyncStructures();
	initDescriptors();
	initPipelines();
	initImgui();
	initDefaultData();

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		_renderData[i].init(&_vulkanDevice);
	}

	mainCamera.velocity = glm::vec3(0.f);
	mainCamera.position = glm::vec3(0, 0, 5);

	mainCamera.pitch = 0;
	mainCamera.yaw = 0;

	//everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized) {

		vkDeviceWaitIdle(_vulkanDevice._logicalDevice);
		for (auto& mesh : testMeshes) {
			_vulkanDevice.destroyBuffer(mesh->meshBuffers.indexBuffer);
			_vulkanDevice.destroyBuffer(mesh->meshBuffers.vertexBuffer);
		}
		_mainDestructor.destroy(&_vulkanDevice, nullptr);

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
			_renderData[i]._frameDestructor.destroy(&_vulkanDevice, nullptr);
			_renderData[i].destroy(&_vulkanDevice);
		}

		_swapchain.destroySwapchain(&_vulkanDevice);


		vkDestroySurfaceKHR(_instance, _surface, nullptr);


		_vulkanDevice.destroy();
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger, nullptr);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw()
{
	updateScene();

	RenderData* currentFrame = &getCurrentRenderData();
	currentFrame->_frameDestructor.destroy(&_vulkanDevice, &currentFrame->_renderFence);
	currentFrame->_frameDescriptors.clearPools(_vulkanDevice._logicalDevice);


	uint32_t swapchainImageIndex;
	VkResult e = vkAcquireNextImageKHR(_vulkanDevice._logicalDevice, _swapchain._swapchain, 1000000000, currentFrame->_swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		SDL_GetWindowSizeInPixels(_window, (int*)&_windowExtent.width, (int*)&_windowExtent.height);
		_swapchain.rebuildSwapchain(&_vulkanDevice, _surface, _windowExtent.height, _windowExtent.width);
		return;
	}

	VK_CHECK(vkResetFences(_vulkanDevice._logicalDevice, 1, &currentFrame->_renderFence));

	VK_CHECK(vkResetCommandBuffer(currentFrame->_mainCommandBuffer, 0));

	VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	_drawExtent.width = _drawImage.imageExtent.width;
	_drawExtent.height = _drawImage.imageExtent.height;


	VK_CHECK(vkBeginCommandBuffer(currentFrame->_mainCommandBuffer, &commandBufferBeginInfo));

	_drawImage.transitionImage(&currentFrame->_mainCommandBuffer, VK_IMAGE_LAYOUT_GENERAL);

	drawBackground(&currentFrame->_mainCommandBuffer);

	_drawImage.transitionImage(&currentFrame->_mainCommandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	_depthImage.transitionImage(&currentFrame->_mainCommandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	drawGeometry(&currentFrame->_mainCommandBuffer);

	_drawImage.transitionImage(&currentFrame->_mainCommandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	_swapchain.transitionSwapchainImage(&currentFrame->_mainCommandBuffer, swapchainImageIndex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	_drawImage.copyToImage(&currentFrame->_mainCommandBuffer, &_swapchain._swapchainImages[swapchainImageIndex], _swapchain._swapchainExtent);

	_swapchain.transitionSwapchainImage(&currentFrame->_mainCommandBuffer, swapchainImageIndex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	drawImgui(&currentFrame->_mainCommandBuffer, _swapchain._swapchainImageViews[swapchainImageIndex]);

	_swapchain.transitionSwapchainImage(&currentFrame->_mainCommandBuffer, swapchainImageIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(currentFrame->_mainCommandBuffer));




	VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::CommandBufferSubmitInfo(currentFrame->_mainCommandBuffer);

	VkSemaphoreSubmitInfo waitInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentRenderData()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentRenderData()._renderSemaphore);

	VkSubmitInfo2 submitInfo = vkinit::SubmitInfo2(&commandBufferSubmitInfo, &signalInfo, &waitInfo);

	VK_CHECK(vkQueueSubmit2(_vulkanDevice.getQueue(VulkanDevice::GRAPHICS), 1, &submitInfo, getCurrentRenderData()._renderFence));
	
	VkPresentInfoKHR presentInfo = vkinit::PresentInfoKHR();

	presentInfo.pSwapchains = &_swapchain._swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &getCurrentRenderData()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(_vulkanDevice.getQueue(VulkanDevice::GRAPHICS), &presentInfo);


	//increase the number of frames drawn
	_frameNumber++;
}

void VulkanEngine::run()
{
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
					stop_rendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
					stop_rendering = false;
				}
			}
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
				if (mainCamera.relMouse == false) {
					SDL_SetRelativeMouseMode(SDL_TRUE);
					mainCamera.relMouse = true;
				}
				else if (mainCamera.relMouse == true) {
					SDL_SetRelativeMouseMode(SDL_FALSE);
					mainCamera.relMouse = false;
				}
			}

			mainCamera.processSDLEvent(e);
			//send SDL event to imgui for handling
			ImGui_ImplSDL2_ProcessEvent(&e);
		}

		//do not draw if we are minimized
		if (stop_rendering) {
			//throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		// imgui background
		ImGui::NewFrame();

		if (ImGui::Begin("background")) {

			ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

			ImGui::Text("Selected effect: ", selected.name);

			ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

			ImGui::InputFloat4("data1", (float*)&selected.data.data1);
			ImGui::InputFloat4("data2", (float*)&selected.data.data2);
			ImGui::InputFloat4("data3", (float*)&selected.data.data3);
			ImGui::InputFloat4("data4", (float*)&selected.data.data4);

			ImGui::End();
		}
		ImGui::Render();

		//// imgui input sensitivity
		//ImGui::NewFrame();

		//if (ImGui::Begin("input sensitivity")) {


		//	ImGui::SliderFloat("movespeed", &mainCamera.moveSpeed, 0, 2);
		//	ImGui::SliderFloat("mouse sense", &mainCamera.mouseSense, 0, 2);

		//	ImGui::End();
		//}
		//ImGui::Render();

		draw();
	}
}



void VulkanEngine::drawBackground(VkCommandBuffer* commandBuffer)
{
	ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(*commandBuffer, _computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
	vkCmdDispatch(*commandBuffer, (uint32_t)std::ceil(_drawExtent.width / 16.0), (uint32_t)std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::drawGeometry(VkCommandBuffer* commandBuffer) {

	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = _vulkanDevice.createBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	getCurrentRenderData()._frameDestructor.buffers.push_back(&gpuSceneDataBuffer);

	//write the buffer
	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
	*sceneUniformData = sceneData;

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = getCurrentRenderData()._frameDescriptors.allocate(_vulkanDevice._logicalDevice, _gpuSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.writeBuffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); // something makes bind 0 a sampler, find it
	writer.updateSet(_vulkanDevice._logicalDevice, globalDescriptor);

	//begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vkinit::RenderingAttachmentInfo(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL); 
	VkRenderingAttachmentInfo depthAttachment = vkinit::DepthAttachmentInfo(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);


	VkRenderingInfo renderInfo = vkinit::RenderingInfo(_drawExtent, &colorAttachment, &depthAttachment);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _drawExtent.width;
	viewport.height = _drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;


	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = _drawExtent.width;
	scissor.extent.height = _drawExtent.height;

	vkCmdBeginRendering(*commandBuffer, &renderInfo);
	for (const RenderObject& draw : mainDrawContext.OpaqueSurfaces) {

		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->pipeline);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 1, 1, &draw.material->materialSet, 0, nullptr);

		vkCmdSetViewport(*commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(*commandBuffer, 0, 1, &scissor);

		vkCmdBindIndexBuffer(*commandBuffer, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		GPUDrawPushConstants pushConstants;
		pushConstants.vertexBuffer = draw.vertexBufferAddress;
		pushConstants.worldMatrix = draw.transform;
		vkCmdPushConstants(*commandBuffer, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

		vkCmdDrawIndexed(*commandBuffer, draw.indexCount, 1, draw.firstIndex, 0, 0);
	}

	vkCmdEndRendering(*commandBuffer);
}

void VulkanEngine::drawImgui(VkCommandBuffer* commandBuffer, VkImageView targetImageView)
{
	VkRenderingAttachmentInfo colorAttachment = vkinit::RenderingAttachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::RenderingInfo(_swapchain._swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(*commandBuffer, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);

	vkCmdEndRendering(*commandBuffer);
}



void VulkanEngine::updateScene()
{
	mainDrawContext.OpaqueSurfaces.clear();

	loadedNodes["Suzanne"]->draw(glm::mat4{ 1.f }, mainDrawContext);

	mainCamera.update();

	glm::mat4 view = mainCamera.getViewMatrix();

	// camera projection
	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	projection[1][1] *= -1;

	sceneData.view = view;
	sceneData.proj = projection;
	sceneData.viewproj = projection * view;

	//some default lighting parameters
	sceneData.ambientColor = glm::vec4(.1f,.1f,.1f,1.f);
	sceneData.sunlightColor = glm::vec4(1.f,1.f,1.f,1.f);
	sceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 10.f);

	for (int x = -3; x < 3; x++) {

		glm::mat4 scale = glm::scale(glm::vec3{ 0.2 });
		glm::mat4 translation = glm::translate(glm::vec3{ x, 1, 0 });

		loadedNodes["Cube"]->draw(translation * scale, mainDrawContext);
	}
}



void VulkanEngine::initVulkan()
{
	vkb::InstanceBuilder vkbInstanceBuilder;

	vkb::Instance vkbInstance = vkbInstanceBuilder
		.set_app_name("Example Vulkan Application")
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build()
		.value();

	_instance = vkbInstance.instance;
	_debug_messenger = vkbInstance.debug_messenger;

	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

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
		.set_surface(_surface)
		.select()
		.value();

	_vulkanDevice.init(vkbPhysicalDevice, _instance);
}

void VulkanEngine::initSwapchain()
{
	_swapchain.createSwapchain(&_vulkanDevice, _surface, _windowExtent.width, _windowExtent.height);

	VkExtent3D drawImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo renderImageCreateInfo = vkinit::ImageCreateInfo(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

	VmaAllocationCreateInfo renderImageAllocationCreateInfo = {};
	renderImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	renderImageAllocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	_drawImage = _vulkanDevice.createImage(drawImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages, VK_IMAGE_ASPECT_COLOR_BIT);

	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	_depthImage = _vulkanDevice.createImage(drawImageExtent, VK_FORMAT_D32_SFLOAT, depthImageUsages, VK_IMAGE_ASPECT_DEPTH_BIT);
	_mainDestructor.images.push_back(&_drawImage);
	_mainDestructor.images.push_back(&_depthImage);
}

void VulkanEngine::initCommands() {}

void VulkanEngine::initSyncStructures() {}

void VulkanEngine::initImgui()
{
	//  the size of the pool is very oversize, but it's copied from imgui demo itself.
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

	VK_CHECK(vkCreateDescriptorPool(_vulkanDevice._logicalDevice, &descriptorPoolCreateInfo, nullptr, &imguiDescriptorPool));

	ImGui::CreateContext();

	ImGui_ImplSDL2_InitForVulkan(_window);

	ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
	imguiInitInfo.Instance = _instance;
	imguiInitInfo.PhysicalDevice = _vulkanDevice._physicalDevice;
	imguiInitInfo.Device = _vulkanDevice._logicalDevice;
	imguiInitInfo.Queue = _vulkanDevice.getQueue(VulkanDevice::GRAPHICS);
	imguiInitInfo.DescriptorPool = imguiDescriptorPool;
	imguiInitInfo.MinImageCount = 3;
	imguiInitInfo.ImageCount = 3;
	imguiInitInfo.UseDynamicRendering = true;

	imguiInitInfo.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	imguiInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	imguiInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchain._swapchainImageFormat;


	imguiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&imguiInitInfo);

	ImGui_ImplVulkan_CreateFontsTexture();
}

void VulkanEngine::initDefaultData() {

	//3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = _vulkanDevice.createImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = _vulkanDevice.createImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));
	_blackImage = _vulkanDevice.createImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = _vulkanDevice.createImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(_vulkanDevice._logicalDevice, &sampl, nullptr, &_defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_vulkanDevice._logicalDevice, &sampl, nullptr, &_defaultSamplerLinear);

	_mainDestructor.samplers.push_back(&_defaultSamplerNearest);
	_mainDestructor.samplers.push_back(&_defaultSamplerLinear);

	_mainDestructor.images.push_back(&_whiteImage);
	_mainDestructor.images.push_back(&_greyImage);
	_mainDestructor.images.push_back(&_blackImage);
	_mainDestructor.images.push_back(&_errorCheckerboardImage);

	testMeshes = loadGLTFMeshes(this, "..\\..\\assets\\basicmesh.glb").value();

	GLTFMetallicRoughness::MaterialResources materialResources;
	//default the material textures
	materialResources.colorImage = _whiteImage;
	materialResources.colorSampler = _defaultSamplerLinear;
	materialResources.metalRoughImage = _whiteImage;
	materialResources.metalRoughSampler = _defaultSamplerLinear;

	//set the uniform buffer for the material data
	AllocatedBuffer materialConstants = _vulkanDevice.createBuffer(sizeof(GLTFMetallicRoughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//write the buffer
	GLTFMetallicRoughness::MaterialConstants* sceneUniformData = (GLTFMetallicRoughness::MaterialConstants*)materialConstants.allocation->GetMappedData();
	sceneUniformData->colorFactors = glm::vec4{ 1,1,1,1 };
	sceneUniformData->metallicRoughnessFactors= glm::vec4{ 1,0.5,0,0 };

	_mainDestructor.buffers.push_back(&materialConstants);

	materialResources.dataBuffer = materialConstants.buffer;
	materialResources.dataBufferOffset = 0;

	defaultData = metalRoughMaterial.writeMaterial(&_vulkanDevice, MaterialPass::MainColor, materialResources, globalDescriptorAllocator);

	for (auto& m : testMeshes) {
		std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
		newNode->mesh = m;

		newNode->localTransform = glm::mat4{ 1.f };
		newNode->worldTransform = glm::mat4{ 1.f }; 

		for (auto& s : newNode->mesh->surfaces) {
			s.material = std::make_shared<GLTFMaterial>(defaultData);
		}

		loadedNodes[m->name] = std::move(newNode);
	}

}

void VulkanEngine::initPipelines()
{
	initBackgroundPipelines();

	//initMeshPipeline();

	metalRoughMaterial.buildPipelines(this);
}

void VulkanEngine::initDescriptors()
{

	std::vector < DescriptorAllocatorGrowable ::PoolSizeRatio > sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
	};

	globalDescriptorAllocator.init(_vulkanDevice._logicalDevice, 10, sizes);

	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		_gpuSceneDataDescriptorLayout = builder.build(_vulkanDevice._logicalDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_singleImageDescriptorLayout = builder.build(_vulkanDevice._logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		_drawImageDescriptorLayout = builder.build(_vulkanDevice._logicalDevice, VK_SHADER_STAGE_COMPUTE_BIT);
	}
	_drawImageDescriptors = globalDescriptorAllocator.allocate(_vulkanDevice._logicalDevice, _drawImageDescriptorLayout);

	{
		DescriptorWriter writer;
		writer.writeImage(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

		writer.updateSet(_vulkanDevice._logicalDevice, _drawImageDescriptors);
	}

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		_renderData[i]._frameDescriptors = DescriptorAllocatorGrowable{};
		_renderData[i]._frameDescriptors.init(_vulkanDevice._logicalDevice, 1000, frame_sizes);
	}
}

void VulkanEngine::initBackgroundPipelines() {
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(ComputePushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_vulkanDevice._logicalDevice, &computeLayout, nullptr, &_computePipelineLayout));

	VkShaderModule gradientShader;
	if (!vkutil::loadShaderModule("../../shaders/gradient_color.comp.spv", _vulkanDevice._logicalDevice, &gradientShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	VkShaderModule skyShader;
	if (!vkutil::loadShaderModule("../../shaders/sky.comp.spv", _vulkanDevice._logicalDevice, &skyShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = gradientShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = _computePipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	ComputeEffect gradient;
	gradient.layout = _computePipelineLayout;
	gradient.name = "gradient";
	gradient.data = {};

	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(_vulkanDevice._logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

	computePipelineCreateInfo.stage.module = skyShader;

	ComputeEffect sky;
	sky.layout = _computePipelineLayout;
	sky.name = "sky";
	sky.data = {};
	sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

	VK_CHECK(vkCreateComputePipelines(_vulkanDevice._logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

	backgroundEffects.push_back(gradient);
	backgroundEffects.push_back(sky);

	vkDestroyShaderModule(_vulkanDevice._logicalDevice, gradientShader, nullptr);
	vkDestroyShaderModule(_vulkanDevice._logicalDevice, skyShader, nullptr);
}

// cleanup commands

void VulkanEngine::cleanupVulkan()
{
	_vulkanDevice.destroy();
	vkb::destroy_debug_utils_messenger(_instance, _debug_messenger, nullptr);
	vkDestroyInstance(_instance, nullptr);
}

void VulkanEngine::cleanupSwapchain()
{
	_swapchain.destroySwapchain(&_vulkanDevice);
}

void VulkanEngine::cleanupCommands() {}

void VulkanEngine::cleanupSyncStructures() {}

void VulkanEngine::cleanupImgui()
{
	ImGui_ImplVulkan_Shutdown();
	vkDestroyDescriptorPool(_vulkanDevice._logicalDevice, imguiDescriptorPool, nullptr);
}

void VulkanEngine::cleanupPipelines() {
	cleanupBackgroundPipelines();
	metalRoughMaterial.destroyPipelines(&_vulkanDevice);
}

void VulkanEngine::cleanupDescriptors()
{
	globalDescriptorAllocator.destroyPools(_vulkanDevice._logicalDevice);

	vkDestroyDescriptorSetLayout(_vulkanDevice._logicalDevice, _gpuSceneDataDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(_vulkanDevice._logicalDevice, _singleImageDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(_vulkanDevice._logicalDevice, _drawImageDescriptorLayout, nullptr);
}

void VulkanEngine::cleanupBackgroundPipelines() {
	vkDestroyPipelineLayout(_vulkanDevice._logicalDevice, _computePipelineLayout, nullptr);
	for (ComputeEffect effect : backgroundEffects) {
		vkDestroyPipeline(_vulkanDevice._logicalDevice, effect.pipeline, nullptr);
	}
}


GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	newSurface.vertexBuffer = _vulkanDevice.createBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_vulkanDevice._logicalDevice, &deviceAdressInfo);

	newSurface.indexBuffer = _vulkanDevice.createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	AllocatedBuffer staging = _vulkanDevice.createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = staging.allocation->GetMappedData();
	// copy vertex buffer
	memcpy(data, vertices.data(), vertexBufferSize);
	// copy index buffer
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	_vulkanDevice.immediateSubmit([&](VkCommandBuffer* commandBuffer) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(*commandBuffer, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(*commandBuffer, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
		});

	_vulkanDevice.destroyBuffer(staging);

	return newSurface;

}











void GLTFMetallicRoughness::buildPipelines(VulkanEngine* engine)
{
	VkShaderModule meshFragShader;
	if (vkutil::loadShaderModule("../../shaders/mesh.frag.spv", engine->_vulkanDevice._logicalDevice, &meshFragShader)) {
		fmt::println("Mesh fragment shader module loaded successfully");
	}
	else {
		fmt::println("Error when building the mesh fragment shader module");
	}
	VkShaderModule meshVertexShader;
	if (vkutil::loadShaderModule("../../shaders/mesh.vert.spv", engine->_vulkanDevice._logicalDevice, &meshVertexShader)) {
		fmt::println("Mesh vertex shader module loaded successfully");
	}
	else {
		fmt::println("Error when building the mesh vertex shader module");

	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	materialLayout = layoutBuilder.build(engine->_vulkanDevice._logicalDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { engine->_gpuSceneDataDescriptorLayout, materialLayout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::PipelineLayoutCreateInfo();
	mesh_layout_info.setLayoutCount = 2;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_vulkanDevice._logicalDevice, &mesh_layout_info, nullptr, &newLayout));

	opaquePipeline.layout = newLayout;
	transparentPipeline.layout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.setShaders(meshVertexShader, meshFragShader);
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.setMultisamplingNone();
	pipelineBuilder.disableBlending();
	pipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//render format
	pipelineBuilder.setColorAttachmentFormat(engine->_drawImage.imageFormat);
	pipelineBuilder.setDepthFormat(engine->_depthImage.imageFormat);

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
	opaquePipeline.pipeline = pipelineBuilder.buildPipeline(engine->_vulkanDevice._logicalDevice);

	// create the transparent variant
	pipelineBuilder.enableBlendingAdditive();

	pipelineBuilder.enableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.buildPipeline(engine->_vulkanDevice._logicalDevice);

	vkDestroyShaderModule(engine->_vulkanDevice._logicalDevice, meshFragShader, nullptr);
	vkDestroyShaderModule(engine->_vulkanDevice._logicalDevice, meshVertexShader, nullptr);
}

void GLTFMetallicRoughness::destroyPipelines(VulkanDevice* device) {
	vkDestroyPipeline(device->_logicalDevice, opaquePipeline.pipeline, nullptr);
	vkDestroyPipeline(device->_logicalDevice, transparentPipeline.pipeline, nullptr);
	vkDestroyPipelineLayout(device->_logicalDevice, opaquePipeline.layout, nullptr);
	vkDestroyPipelineLayout(device->_logicalDevice, transparentPipeline.layout, nullptr);
}

MaterialInstance GLTFMetallicRoughness::writeMaterial(VulkanDevice* device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance matData;
	matData.passType = pass;
	if (pass == MaterialPass::Transparent) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = descriptorAllocator.allocate(device->_logicalDevice, materialLayout);


	writer.clear();
	writer.writeBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.writeImage(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.writeImage(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.updateSet(device->_logicalDevice, matData.materialSet);

	return matData;
}







void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
	glm::mat4 nodeMatrix = topMatrix * worldTransform;

	for (auto& s : mesh->surfaces) {
		RenderObject def;
		def.indexCount = s.count;
		def.firstIndex = s.startIndex;
		def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
		def.material = &s.material->data;

		def.transform = nodeMatrix;
		def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

		ctx.OpaqueSurfaces.push_back(def);
	}

	// recurse down
	Node::draw(topMatrix, ctx);
}