#pragma once

#include "vk_types.h"
#include "vk_device.h"
#include "vk_resources.h"
#include "vk_descriptors.h"
#include "vk_gltf.h"
#include "vk_swapchain.h"
#include "camera.h"



struct FrameData {
	VkSemaphore swapchainSemaphore, renderSemaphore;
	VkFence renderFence;
	
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;
	DescriptorAllocatorGrowable frameDescriptors;

	void destroy(VulkanDevice* device);
	void init(VulkanDevice* device);
};

class VulkanEngine {
public:
	uint64_t frameCount;
	float deltaTime;
	bool rendering{ false };

	VkExtent2D windowExtent{ 1920 , 1080 };
	struct SDL_Window* window{ nullptr };

	FrameData& getCurrentFrameData() { return framesData[frameCount % FRAMES_IN_FLIGHT]; };

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VulkanDevice vulkanDevice;
	VulkanSwapchain vulkanSwapchain;
	VkSurfaceKHR surface;

	FrameData framesData[FRAMES_IN_FLIGHT];

	Camera camera;

	VkDescriptorPool imguiDescriptorPool;

	DescriptorAllocatorGrowable globalDescriptorAllocator;
	VkDescriptorSetLayout sceneDataDescriptorLayout;

	VkDescriptorSet drawImageDescriptors;
	VkDescriptorSetLayout drawImageDescriptorLayout;
	AllocatedImage drawImage;
	VkExtent2D drawExtent;
	
	AllocatedImage depthImage;

	VulkanPipeline defaultPipeline;

	MetallicRoughnessMaterialWriter materialWriter;

	std::vector<VulkanGLTFModel> models;

	SceneData sceneData;
	AllocatedBuffer sceneDataBuffer;
	void* sceneDataAllocation;


	DrawObjectCollection drawObjectCollection;

	void init();
	void run();
	void draw();
	void destroy();
private:
	void initVulkan();
	void initSwapchain();
	void initImgui();
	void initPipelines();
	void initDescriptors();
	void initData();

	void drawGeometry(VkCommandBuffer* commandBuffer, FrameData* frame);
	void appendSceneDraws();
};