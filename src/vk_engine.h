#pragma once

#include "vk_types.h"
#include "vk_device.h"
#include "vk_swapchain.h"


class VulkanEngine {
public:
	uint64_t frameCount;
	float deltaTime;
	bool rendering{ false };

	VkExtent2D windowExtent{ 1920 , 1080 };
	struct SDL_Window* window{ nullptr };

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VulkanDevice vulkanDevice;
	VulkanSwapchain swapchain;
	VkSurfaceKHR surface;

	void init();
	void run();
	void draw();
	void destroy();
private:
	void initVulkan();
	void initSwapchain();
};