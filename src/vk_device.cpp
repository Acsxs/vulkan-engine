
#include "vk_device.h"
#include <iostream>

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

AllocatedBuffer VulkanDevice::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {

	// allocate buffer
	VkBufferCreateInfo bufferCreateInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.pNext = nullptr;
	bufferCreateInfo.size = allocSize;

	bufferCreateInfo.usage = usage;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.usage = memoryUsage;
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	AllocatedBuffer newBuffer = {};

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

	return newBuffer;
}

void VulkanDevice::destroyBuffer(const AllocatedBuffer& buffer) {
	vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}


AllocatedImage VulkanDevice::createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool mipmapped)
{
	AllocatedImage newImage;
	newImage.imageFormat = format;
	newImage.imageExtent = size;
	newImage.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImageCreateInfo img_info = vkinit::ImageCreateInfo(format, usage, size);
	if (mipmapped) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	//VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	//if (format == VK_FORMAT_D32_SFLOAT) {
	//	aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	//}

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::ImageViewCreateInfo(format, newImage.image, aspect);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK(vkCreateImageView(logicalDevice, &view_info, nullptr, &newImage.imageView));

	return newImage;
}

AllocatedImage VulkanDevice::createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, bool mipmapped)
{
	size_t dataSize = size.depth * size.width * size.height * 4;
	AllocatedBuffer uploadBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	uploadBuffer.uploadData(data, dataSize);

	AllocatedImage newImage = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, aspect, mipmapped);

	immediateSubmit([&](VkCommandBuffer* commandBuffer) { uploadBuffer.copyToImage(commandBuffer, newImage, 0, 0, 0, aspect, size); }, GRAPHICS);

	destroyBuffer(uploadBuffer);

	return newImage;
}

void VulkanDevice::destroyImage(const AllocatedImage& img)
{
	vkDestroyImageView(logicalDevice, img.imageView, nullptr);
	vmaDestroyImage(allocator, img.image, img.allocation);
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

