#include "vk_device.h"

void VulkanDevice::init(vkb::PhysicalDevice vkbPhysicalDevice, VkInstance instance) {

	vkb::DeviceBuilder vkbDeviceBuilder{ vkbPhysicalDevice };

	vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

	logicalDevice = vkbDevice.device;
	physicalDevice = vkbPhysicalDevice.physical_device;

	auto graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics);
	if (!graphicsQueue) {
		std::cerr << "Failed to get graphics queue. Error: " << graphicsQueue.error().message() << "\n";
		throw;
	}
	auto transferQueue = vkbDevice.get_queue(vkb::QueueType::transfer);
	if (!transferQueue) {
		std::cerr << "Failed to get transfer queue. Error: " << transferQueue.error().message() << "\n";
		queues[1] = queues[0];
		queueFamilies[1] = queueFamilies[0];
	}
	else {
		queues[1] = std::make_shared<VkQueue>(transferQueue.value());
		queueFamilies[1] = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();
	}
	auto computeQueue = vkbDevice.get_queue(vkb::QueueType::compute);
	if (!transferQueue) {
		std::cerr << "Failed to get compute queue. Error: " << computeQueue.error().message() << "\n";
		queues[2] = queues[0];
		queueFamilies[2] = queueFamilies[0];
	}
	else {
		queues[2] = std::make_shared<VkQueue>(computeQueue.value());
		queueFamilies[2] = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
	}

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


void VulkanDevice::immediateSubmit(std::function<void(VkCommandBuffer* commandBuffer)>&& function, queueType queueType)
{
	VK_CHECK(vkResetFences(logicalDevice, 1, &immFence));
	VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(immCommandBuffer, &cmdBeginInfo));

	function(&immCommandBuffer);

	VK_CHECK(vkEndCommandBuffer(immCommandBuffer));

	VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::CommandBufferSubmitInfo(immCommandBuffer);
	VkSubmitInfo2 submitInfo = vkinit::SubmitInfo2(&commandBufferSubmitInfo, nullptr, nullptr);

	VK_CHECK(vkQueueSubmit2(*queues[queueType].get(), 1, &submitInfo, immFence));

	VK_CHECK(vkWaitForFences(logicalDevice, 1, &immFence, true, 9999999999));
}

