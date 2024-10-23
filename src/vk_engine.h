// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_device.h"
#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_swapchain.h"
#include "vk_resources.h"
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
	float runtime = 0;
	bool rendering{ false };

	VkExtent2D windowExtent{ 1920 , 1080 };
	struct SDL_Window* window{ nullptr };

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VulkanDevice vulkanDevice;
	VulkanSwapchain vulkanSwapchain;
	VkSurfaceKHR surface;

	VkExtent2D drawExtent;

	FrameData framesData[FRAMES_IN_FLIGHT];
	FrameData& getCurrentFrameData() { return framesData[frameCount % FRAMES_IN_FLIGHT]; };

	Camera camera;

	VkDescriptorPool imguiDescriptorPool;

	DescriptorAllocatorGrowable globalDescriptorAllocator;

	VkDescriptorSet drawImageDescriptors;
	VkDescriptorSetLayout drawImageDescriptorLayout;
	AllocatedImage drawImage;
	
	AllocatedImage depthImage;

	VulkanPipeline defaultPipeline;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	SceneData sceneData;
	AllocatedBuffer sceneDataBuffer;
	VkDescriptorSetLayout sceneDataDescriptorLayout;

	AllocatedBuffer indexBuffer;
	AllocatedBuffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress;

	void init();
	void run();
	void draw();
	void destroy();
private:
	void drawGeometry(VkCommandBuffer* commandBuffer, FrameData* frame);
	void initVulkan();
	void initSwapchain();
	void initPipelines();
	void initDescriptors();
	void initDummyData();

};