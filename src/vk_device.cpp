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
	queues[GRAPHICS] = std::make_shared<VkQueue>(graphicsQueue.value());
	queueFamilies[GRAPHICS] = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	auto transferQueue = vkbDevice.get_queue(vkb::QueueType::transfer);
	if (!transferQueue) {
		std::cerr << "[Warn] Failed to get transfer queue: " << transferQueue.error().message() << "\n";
		queues[TRANSFER] = queues[GRAPHICS];
		queueFamilies[TRANSFER] = queueFamilies[GRAPHICS];
	}
	else {
		queues[TRANSFER] = std::make_shared<VkQueue>(transferQueue.value());
		queueFamilies[TRANSFER] = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();
	}
	auto computeQueue = vkbDevice.get_queue(vkb::QueueType::compute);
	if (!transferQueue) {
		std::cerr << "[Warn] Failed to get compute queue: " << computeQueue.error().message() << "\n";
		queues[TRANSFER] = queues[GRAPHICS];
		queueFamilies[TRANSFER] = queueFamilies[GRAPHICS];
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

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = vkinit::CommandPoolCreateInfo(getQueueFamilyIndex(GRAPHICS), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(logicalDevice, &graphicsCommandPoolCreateInfo, nullptr, &immCommandPool[GRAPHICS]));
	VkCommandBufferAllocateInfo graphicsCommandBufferAllocateInfo = vkinit::CommandBufferAllocateInfo(immCommandPool[GRAPHICS], 1);
	VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &graphicsCommandBufferAllocateInfo, &immCommandBuffer[GRAPHICS]));

	VkCommandPoolCreateInfo transferCommandPoolCreateInfo = vkinit::CommandPoolCreateInfo(getQueueFamilyIndex(TRANSFER), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(logicalDevice, &transferCommandPoolCreateInfo, nullptr, &immCommandPool[TRANSFER]));
	VkCommandBufferAllocateInfo transferCommandBufferAllocateInfo = vkinit::CommandBufferAllocateInfo(immCommandPool[TRANSFER], 1);
	VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &transferCommandBufferAllocateInfo, &immCommandBuffer[TRANSFER]));

	VkCommandPoolCreateInfo computeCommandPoolCreateInfo = vkinit::CommandPoolCreateInfo(getQueueFamilyIndex(COMPUTE), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(logicalDevice, &computeCommandPoolCreateInfo, nullptr, &immCommandPool[COMPUTE]));
	VkCommandBufferAllocateInfo computeCommandBufferAllocateInfo = vkinit::CommandBufferAllocateInfo(immCommandPool[COMPUTE], 1);
	VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &computeCommandBufferAllocateInfo, &immCommandBuffer[COMPUTE]));


	VkFenceCreateInfo fenceCreateInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::SemaphoreCreateInfo();
	VK_CHECK(vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &immFence));
}

void VulkanDevice::destroy() {
	vkDeviceWaitIdle(logicalDevice);
	for (int i = 0; i < 3; i++) {
		vkDestroyCommandPool(logicalDevice, immCommandPool[i], nullptr);
	}
	vmaDestroyAllocator(allocator);
	vkDestroyDevice(logicalDevice, nullptr);
}


void VulkanDevice::immediateSubmit(std::function<void(VkCommandBuffer* commandBuffer)>&& function, queueType queueType)
{
	VK_CHECK(vkResetFences(logicalDevice, 1, &immFence));
	VK_CHECK(vkResetCommandBuffer(immCommandBuffer[queueType], 0));

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(immCommandBuffer[queueType], &cmdBeginInfo));

	function(&immCommandBuffer[queueType]);

	VK_CHECK(vkEndCommandBuffer(immCommandBuffer[queueType]));

	VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::CommandBufferSubmitInfo(immCommandBuffer[queueType]);
	VkSubmitInfo2 submitInfo = vkinit::SubmitInfo2(&commandBufferSubmitInfo, nullptr, nullptr);

	VK_CHECK(vkQueueSubmit2(*queues[queueType].get(), 1, &submitInfo, immFence));

	VK_CHECK(vkWaitForFences(logicalDevice, 1, &immFence, true, 9999999999));
}

