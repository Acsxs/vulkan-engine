
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>
#include <vk_pipelines.h>

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

	initVulkan();

	initSwapchain();

	initCommands();

	initSyncStructures();

	initDescriptors();

	initPipelines();

	initImgui();

	//everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized) {

		vkDeviceWaitIdle(_device);

		_mainDeletionQueue.flush();
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {

			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);

			_frames[i]._deletionQueue.flush();
		}

		destroySwapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);

		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw()
{

	FrameData* currentFrame = &getCurrentFrame();
	VK_CHECK(vkWaitForFences(_device, 1, &currentFrame->_renderFence, true, 1000000000));

	currentFrame->_deletionQueue.flush();

	uint32_t swapchainImageIndex;

	VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, currentFrame->_swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		rebuildSwapchain();
		return;
	}

	VK_CHECK(vkResetFences(_device, 1, &currentFrame->_renderFence));

	VK_CHECK(vkResetCommandBuffer(currentFrame->_mainCommandBuffer, 0));

	VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	_drawExtent.width = _drawImage.imageExtent.width;
	_drawExtent.height = _drawImage.imageExtent.height;

	VK_CHECK(vkBeginCommandBuffer(currentFrame->_mainCommandBuffer, &commandBufferBeginInfo));

	vkutil::transitionImage(&currentFrame->_mainCommandBuffer, &_drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	drawBackground(&currentFrame->_mainCommandBuffer);

	vkutil::transitionImage(&currentFrame->_mainCommandBuffer, &_drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	drawGeometry(&currentFrame->_mainCommandBuffer);


	vkutil::transitionImage(&currentFrame->_mainCommandBuffer, &_drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transitionImage(&currentFrame->_mainCommandBuffer, &_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkutil::copyImageToImage(&currentFrame->_mainCommandBuffer, _drawImage.image, &_swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

	vkutil::transitionImage(&currentFrame->_mainCommandBuffer, &_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	drawImgui(&currentFrame->_mainCommandBuffer, _swapchainImageViews[swapchainImageIndex]);

	vkutil::transitionImage(&currentFrame->_mainCommandBuffer, &_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(currentFrame->_mainCommandBuffer));


	VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::CommandBufferSubmitInfo(currentFrame->_mainCommandBuffer);

	VkSemaphoreSubmitInfo waitInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame()._renderSemaphore);

	VkSubmitInfo2 submitInfo = vkinit::SubmitInfo2(&commandBufferSubmitInfo, &signalInfo, &waitInfo);

	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, getCurrentFrame()._renderFence));
	
	VkPresentInfoKHR presentInfo = vkinit::PresentInfoKHR();

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &getCurrentFrame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);


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

		//> imgui_bk
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

		//< imgui_bk

		draw();
	}
}



void VulkanEngine::drawBackground(VkCommandBuffer* commandBuffer)
{
	ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(*commandBuffer, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
	vkCmdDispatch(*commandBuffer, (uint32_t)std::ceil(_drawExtent.width / 16.0), (uint32_t)std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::drawGeometry(VkCommandBuffer* commandBuffer) {
	//begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vkinit::RenderingAttachmentInfo(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = vkinit::RenderingInfo(_drawExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(*commandBuffer, &renderInfo);

	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);

	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _drawExtent.width;
	viewport.height = _drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(*commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = _drawExtent.width;
	scissor.extent.height = _drawExtent.height;

	vkCmdSetScissor(*commandBuffer, 0, 1, &scissor);

	//launch a draw command to draw 3 vertices
	vkCmdDraw(*commandBuffer, 3, 1, 0, 0);

	vkCmdEndRendering(*commandBuffer);
}

void VulkanEngine::drawImgui(VkCommandBuffer* commandBuffer, VkImageView targetImageView)
{
	VkRenderingAttachmentInfo colorAttachment = vkinit::RenderingAttachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::RenderingInfo(_swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(*commandBuffer, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);

	vkCmdEndRendering(*commandBuffer);
}




void VulkanEngine::destroySwapchain()
{
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < _swapchainImageViews.size(); i++) {

		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	}
}

void VulkanEngine::createSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ _physicalDevice,_device,_surface };

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	_swapchainExtent = vkbSwapchain.extent;
	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::rebuildSwapchain()
{
	vkQueueWaitIdle(_graphicsQueue);

	vkb::SwapchainBuilder vkbSwapchainBuilder{ _physicalDevice,_device,_surface };

	SDL_GetWindowSizeInPixels(_window, (int*)&_windowExtent.width, (int*)&_windowExtent.height);

	vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	vkDestroyImageView(_device, _drawImage.imageView, nullptr);
	vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

	vkb::Swapchain vkbSwapchain = vkbSwapchainBuilder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // Swap to mailbox when program runs slower
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

	_swapchainImageFormat = vkbSwapchain.image_format;

	VkExtent3D drawImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	//hardcoding the depth format to 32 bit float
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;

	VkImageCreateInfo renderImageCreateInfo = vkinit::ImageCreateInfo(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

	VmaAllocationCreateInfo renderImageAllocationCreateInfo = {};
	renderImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	renderImageAllocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(_allocator, &renderImageCreateInfo, &renderImageAllocationCreateInfo, &_drawImage.image, &_drawImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo renderImageViewCreateInfo = vkinit::ImageViewCreateInfo(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(_device, &renderImageViewCreateInfo, nullptr, &_drawImage.imageView));

	VkDescriptorImageInfo descriptorImageInfo{};
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	descriptorImageInfo.imageView = _drawImage.imageView;

	VkWriteDescriptorSet cameraWrite = vkinit::WriteDescriptorImage(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, _drawImageDescriptors, &descriptorImageInfo, 0);

	vkUpdateDescriptorSets(_device, 1, &cameraWrite, 0, nullptr);

	//add to deletion queues
	_mainDeletionQueue.pushFunction([&]() {
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
		});
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


	vkb::DeviceBuilder vkbDeviceBuilder{ vkbPhysicalDevice };

	vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

	_device = vkbDevice.device;
	_physicalDevice = vkbPhysicalDevice.physical_device;

	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();

	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.physicalDevice = _physicalDevice;
	allocatorCreateInfo.device = _device;
	allocatorCreateInfo.instance = _instance;
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorCreateInfo, &_allocator);

	_mainDeletionQueue.pushFunction([&]() {
		vmaDestroyAllocator(_allocator);
		});
}

void VulkanEngine::initSwapchain()
{
	createSwapchain(_windowExtent.width, _windowExtent.height);

	VkExtent3D drawImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo renderImageCreateInfo = vkinit::ImageCreateInfo(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

	VmaAllocationCreateInfo renderImageAllocationCreateInfo = {};
	renderImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	renderImageAllocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(_allocator, &renderImageCreateInfo, &renderImageAllocationCreateInfo, &_drawImage.image, &_drawImage.allocation, nullptr);

	VkImageViewCreateInfo renderImageViewCreateInfo = vkinit::ImageViewCreateInfo(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(_device, &renderImageViewCreateInfo, nullptr, &_drawImage.imageView));

	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
		});
}

void VulkanEngine::initCommands()
{
	VkCommandPoolCreateInfo commandPoolCreateInfo = vkinit::CommandPoolCreateInfo(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {

		VK_CHECK(vkCreateCommandPool(_device, &commandPoolCreateInfo, nullptr, &_frames[i]._commandPool));

		VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkinit::CommandBufferAllocateInfo(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &_frames[i]._mainCommandBuffer));
	}

	VK_CHECK(vkCreateCommandPool(_device, &commandPoolCreateInfo, nullptr, &_immCommandPool));

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkinit::CommandBufferAllocateInfo(_immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &_immCommandBuffer));

	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyCommandPool(_device, _immCommandPool, nullptr);
		});
}

void VulkanEngine::initSyncStructures()
{
	VkFenceCreateInfo fenceCreateInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::SemaphoreCreateInfo();

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
	}

	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
	_mainDeletionQueue.pushFunction([=]() { vkDestroyFence(_device, _immFence, nullptr); });
}

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

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &descriptorPoolCreateInfo, nullptr, &imguiPool));

	ImGui::CreateContext();

	ImGui_ImplSDL2_InitForVulkan(_window);

	ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
	imguiInitInfo.Instance = _instance;
	imguiInitInfo.PhysicalDevice = _physicalDevice;
	imguiInitInfo.Device = _device;
	imguiInitInfo.Queue = _graphicsQueue;
	imguiInitInfo.DescriptorPool = imguiPool;
	imguiInitInfo.MinImageCount = 3;
	imguiInitInfo.ImageCount = 3;
	imguiInitInfo.UseDynamicRendering = true;

	imguiInitInfo.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	imguiInitInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	imguiInitInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;


	imguiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&imguiInitInfo);

	ImGui_ImplVulkan_CreateFontsTexture();

	_mainDeletionQueue.pushFunction([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		});
}

void VulkanEngine::initPipelines()
{
	initTrianglePipeline();

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

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

	VkShaderModule gradientShader;
	if (!vkutil::loadShaderModule("../../shaders/gradient_color.comp.spv", _device, &gradientShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	VkShaderModule skyShader;
	if (!vkutil::loadShaderModule("../../shaders/sky.comp.spv", _device, &skyShader)) {
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
	computePipelineCreateInfo.layout = _gradientPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	ComputeEffect gradient;
	gradient.layout = _gradientPipelineLayout;
	gradient.name = "gradient";
	gradient.data = {};

	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

	computePipelineCreateInfo.stage.module = skyShader;

	ComputeEffect sky;
	sky.layout = _gradientPipelineLayout;
	sky.name = "sky";
	sky.data = {};
	sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

	backgroundEffects.push_back(gradient);
	backgroundEffects.push_back(sky);

	vkDestroyShaderModule(_device, gradientShader, nullptr);
	vkDestroyShaderModule(_device, skyShader, nullptr);
	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
		vkDestroyPipeline(_device, sky.pipeline, nullptr);
		vkDestroyPipeline(_device, gradient.pipeline, nullptr);
		});

}

void VulkanEngine::initDescriptors()
{
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};

	globalDescriptorAllocator.initPool(_device, 10, sizes);

	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		_drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}
	_drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo.imageView = _drawImage.imageView;

	VkWriteDescriptorSet drawImageWrite = {};
	drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	drawImageWrite.pNext = nullptr;

	drawImageWrite.dstBinding = 0;
	drawImageWrite.dstSet = _drawImageDescriptors;
	drawImageWrite.descriptorCount = 1;
	drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	drawImageWrite.pImageInfo = &imgInfo;

	vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);
	_mainDeletionQueue.pushFunction([&]() {
		globalDescriptorAllocator.destroyPool(_device);

		vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
		});
}

void VulkanEngine::initTrianglePipeline() {
	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::PipelineLayoutCreateInfo();
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

	VkShaderModule triangleFragShader;
	if (!vkutil::loadShaderModule("../../shaders/coloured_triangle.frag.spv", _device, &triangleFragShader)) {
		fmt::print("Error when building the triangle fragment shader module");
	}
	else {
		fmt::print("Triangle fragment shader succesfully loaded");
	}

	VkShaderModule triangleVertexShader;
	if (!vkutil::loadShaderModule("../../shaders/coloured_triangle.vert.spv", _device, &triangleVertexShader)) {
		fmt::print("Error when building the triangle vertex shader module");
	}
	else {
		fmt::print("Triangle vertex shader succesfully loaded");
	}

	PipelineBuilder pipelineBuilder;

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.setShaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.setMultisamplingNone();
	//no blending
	pipelineBuilder.disableBlending();
	//no depth testing
	pipelineBuilder.disableDepthtest();

	//connect the image format we will draw into, from draw image
	pipelineBuilder.setColorAttachmentFormat(_drawImage.imageFormat);
	pipelineBuilder.setDepthFormat(VK_FORMAT_UNDEFINED);

	//finally build the pipeline
	_trianglePipeline = pipelineBuilder.buildPipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.pushFunction([&]() {
		vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
		vkDestroyPipeline(_device, _trianglePipeline, nullptr);
		});
}



void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer* commandBuffer)>&& function)
{
	VK_CHECK(vkResetFences(_device, 1, &_immFence));
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(_immCommandBuffer, &cmdBeginInfo));

	function(&_immCommandBuffer);

	VK_CHECK(vkEndCommandBuffer(_immCommandBuffer));

	VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::CommandBufferSubmitInfo(_immCommandBuffer);
	VkSubmitInfo2 submitInfo = vkinit::SubmitInfo2(&commandBufferSubmitInfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, _immFence));

	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}
