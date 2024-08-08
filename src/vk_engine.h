// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>



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

	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;

	std::vector<VkFramebuffer> _framebuffers;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	DeletionQueue _mainDeletionQueue;

	VmaAllocator _allocator; //vma lib allocator
	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	//draw resources
	AllocatedImage _drawImage;

	std::vector<ComputeEffect> backgroundEffects;

	int currentBackgroundEffect{ 0 };
	//initializes everything in the engine
	VulkanEngine& Get();
	
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	void drawGeometry(VkCommandBuffer* commandBuffer);
	void drawBackground(VkCommandBuffer* commandBuffer);
	void drawImgui(VkCommandBuffer* commandBuffer,  VkImageView targetImageView);

	//run main loop
	void run();

	void immediateSubmit(std::function<void(VkCommandBuffer* commandBuffer)>&& function);

private:

	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initPipelines();
	void initDescriptors();
	void initSyncStructures();
	void initImgui();
	void initTrianglePipeline();

	void rebuildSwapchain();
	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain();
};