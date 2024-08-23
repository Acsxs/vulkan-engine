#pragma once

#include "vk_types.h"
#include "vk_device.h"

struct VulkanSwapchain {
	VkSurfaceKHR _surface;
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	VkExtent2D _swapchainExtent;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;

	void destroySwapchain(VulkanDevice* device);
	void createSwapchain(VulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
	void rebuildSwapchain(VulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
	void transitionImage(
		VkCommandBuffer* commandBuffer, 
		uint32_t imageIndex,
		VkImageLayout currentLayout,
		VkImageLayout newLayout,
		VkPipelineStageFlagBits2 srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		VkPipelineStageFlagBits2 dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		VkAccessFlagBits2 srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
		VkAccessFlagBits2 dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT);
};