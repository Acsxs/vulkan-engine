#include "vk_swapchain.h"
#include <VkBootstrap.h>


void VulkanSwapchain::destroySwapchain(VulkanDevice* device)
{
	vkDestroySwapchainKHR(device->_logicalDevice, _swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < _swapchainImageViews.size(); i++) {
		vkDestroyImage(device->_logicalDevice, _swapchainImages[i], nullptr);
		vkDestroyImageView(device->_logicalDevice, _swapchainImageViews[i], nullptr);
	}
}

void VulkanSwapchain::createSwapchain(VulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ device->_physicalDevice, device->_logicalDevice, surface };

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	VkSurfaceFormatKHR surfaceFormat = { .format = _swapchainImageFormat,.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(surfaceFormat)
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // Swap to mailbox when program runs slower
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

void VulkanSwapchain::rebuildSwapchain(VulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height)
{
	vkQueueWaitIdle(device->getQueue(VulkanDevice::GRAPHICS));
	destroySwapchain(device);
	createSwapchain(device, surface, width, height);
}

void VulkanSwapchain::transitionSwapchainImage(
    VkCommandBuffer* commandBuffer,
    uint32_t imageIndex,
    VkImageLayout currentLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlagBits2 srcStageMask,
    VkPipelineStageFlagBits2 dstStageMask,
    VkAccessFlagBits2 srcAccessMask,
    VkAccessFlagBits2 dstAccessMask
)
{
    VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = srcStageMask;
    imageBarrier.dstStageMask = dstStageMask;
    imageBarrier.srcAccessMask = srcAccessMask;
    imageBarrier.dstAccessMask = dstAccessMask;

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    imageBarrier.subresourceRange = vkinit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    imageBarrier.image = _swapchainImages[imageIndex];

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pNext = nullptr;

    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(*commandBuffer, &dependencyInfo);
}