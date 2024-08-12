// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>
#include <vk_loader.h>



struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void pushFunction(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct FrameData {
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	DeletionQueue _deletionQueue;
};

constexpr unsigned int FRAMES_IN_FLIGHT = 2;

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};



class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{false};
	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;

	FrameData _frames[FRAMES_IN_FLIGHT];

	FrameData& getCurrentFrame() { return _frames[_frameNumber % FRAMES_IN_FLIGHT]; };


	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;	

	VkSurfaceKHR _surface;
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	VkExtent2D _swapchainExtent;
	VkExtent2D _drawExtent;
	
	DescriptorAllocator globalDescriptorAllocator;

	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;
	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;

	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	std::vector<VkFramebuffer> _framebuffers;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;
	AllocatedImage _drawImage;

	AllocatedImage _depthImage;

	DeletionQueue _mainDeletionQueue;

	VmaAllocator _allocator; //vma lib allocator

	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;



	VulkanEngine& Get();
	
	void init();

	void cleanup();

	void draw();

	void drawGeometry(VkCommandBuffer* commandBuffer);
	void drawBackground(VkCommandBuffer* commandBuffer);
	void drawImgui(VkCommandBuffer* commandBuffer,  VkImageView targetImageView);

	void run();

	void immediateSubmit(std::function<void(VkCommandBuffer* commandBuffer)>&& function);
	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:

	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initPipelines();
	void initDescriptors();
	void initSyncStructures();
	void initImgui();
	void initDefaultData();
	void initBackgroundPipelines();
	void initMeshPipeline();

	void rebuildSwapchain();
	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain();
};