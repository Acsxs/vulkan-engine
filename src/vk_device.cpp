#include "vk_device.h"

void VulkanDevice::init(vkb::PhysicalDevice vkbPhysicalDevice, VkInstance instance) {

	vkb::DeviceBuilder vkbDeviceBuilder{ vkbPhysicalDevice };

	vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

	logicalDevice = vkbDevice.device;
	physicalDevice = vkbPhysicalDevice.physical_device;

	queues[0] = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	queueFamilies[0] = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	queues[1] = vkbDevice.get_queue(vkb::QueueType::transfer).value();
	queueFamilies[1] = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();

	queues[2] = vkbDevice.get_queue(vkb::QueueType::compute).value();
	queueFamilies[2] = vkbDevice.get_queue_index(vkb::QueueType::compute).value();

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.physicalDevice = physicalDevice;
	allocatorCreateInfo.device = logicalDevice;
	allocatorCreateInfo.instance = instance;
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorCreateInfo, &allocator);

	VkCommandPoolCreateInfo commandPoolCreateInfo = vkinit::CommandPoolCreateInfo(getQueueFamilyIndex(GRAPHICS), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(logicalDevice, &commandPoolCreateInfo, nullptr, &immCommandPool));

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkinit::CommandBufferAllocateInfo(immCommandPool, 1);
	VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, &immCommandBuffer));

	VkFenceCreateInfo fenceCreateInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::SemaphoreCreateInfo();
	VK_CHECK(vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &immFence));
}

void VulkanDevice::destroy() {
	vkDeviceWaitIdle(logicalDevice);
	vkDestroyCommandPool(logicalDevice, immCommandPool, nullptr);
	vmaDestroyAllocator(allocator);
	vkDestroyDevice(logicalDevice, nullptr);
}

void VulkanDevice::immediateSubmit(std::function<void(VkCommandBuffer* commandBuffer)>&& function)
{
	VK_CHECK(vkResetFences(logicalDevice, 1, &immFence));
	VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(immCommandBuffer, &cmdBeginInfo));

	function(&immCommandBuffer);

	VK_CHECK(vkEndCommandBuffer(immCommandBuffer));

	VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::CommandBufferSubmitInfo(immCommandBuffer);
	VkSubmitInfo2 submitInfo = vkinit::SubmitInfo2(&commandBufferSubmitInfo, nullptr, nullptr);

	VK_CHECK(vkQueueSubmit2(queues[GRAPHICS], 1, &submitInfo, immFence));

	VK_CHECK(vkWaitForFences(logicalDevice, 1, &immFence, true, 9999999999));
}