// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_device.h"
#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "vk_destructor.h"
#include "vk_swapchain.h"
#include "camera.h"



//
//struct MeshNode : public Node {
//
//	std::shared_ptr<MeshAsset> mesh;
//
//	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
//};
//
//struct RenderObject {
//	uint32_t indexCount;
//	uint32_t firstIndex;
//	VkBuffer indexBuffer;
//
//	MaterialInstance* material;
//
//	glm::mat4 transform;
//	VkDeviceAddress vertexBufferAddress;
//};
//
//struct DrawContext {
//	std::vector<RenderObject> OpaqueSurfaces;
//};
//
//
//struct RenderData {
//	VkSemaphore _swapchainSemaphore, _renderSemaphore;
//	VkFence _renderFence;
//
//	VkCommandPool _commandPool;
//	VkCommandBuffer _mainCommandBuffer;
//
//	ResourceDestructor _frameDestructor;
//	DescriptorAllocatorGrowable _frameDescriptors;
//	void destroy(VulkanDevice* device);
//	void init(VulkanDevice* device);
//};
//
//constexpr unsigned int FRAMES_IN_FLIGHT = 2;
//
//struct ComputePushConstants {
//	glm::vec4 data1;
//	glm::vec4 data2;
//	glm::vec4 data3;
//	glm::vec4 data4;
//};
//
//struct ComputeEffect {
//	const char* name;
//
//	VkPipeline pipeline;
//	VkPipelineLayout layout;
//
//	ComputePushConstants data;
//};

//class VulkanEngine {
//public:
//
//	bool _isInitialized{ false };
//	int _frameNumber {0};
//	bool stop_rendering{false};
//	VkExtent2D _windowExtent{ 1920 , 1080 };
//
//	struct SDL_Window* _window{ nullptr };
//
//	VkInstance _instance;
//	VkDebugUtilsMessengerEXT _debug_messenger;
//
//	VulkanDevice _vulkanDevice;
//	ResourceDestructor _mainDestructor;
//
//	RenderData _renderData[FRAMES_IN_FLIGHT];
//
//	RenderData& getCurrentRenderData() { return _renderData[_frameNumber % FRAMES_IN_FLIGHT]; };
//
//
//
//	DrawContext mainDrawContext;
//	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;
//
//	VkDescriptorPool imguiDescriptorPool;
//
//
//	VkSurfaceKHR _surface;
//	
//	VulkanSwapchain _swapchain;
//
//	VkExtent2D _drawExtent;
//
//	DescriptorAllocatorGrowable globalDescriptorAllocator;
//
//	VkPipeline _computePipeline;
//	VkPipelineLayout _computePipelineLayout;
//	std::vector<ComputeEffect> backgroundEffects;
//	int currentBackgroundEffect{ 0 };
//
//	GPUSceneData sceneData;
//	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
//
//	std::vector<std::shared_ptr<MeshAsset>> testMeshes;
//
//	std::vector<VkFramebuffer> _framebuffers;
//
//	VkDescriptorSet _drawImageDescriptors;
//	VkDescriptorSetLayout _drawImageDescriptorLayout;
//	AllocatedImage _drawImage;
//
//	AllocatedImage _depthImage;
//
//	VkDescriptorSetLayout _singleImageDescriptorLayout;
//
//	AllocatedImage _whiteImage;
//	AllocatedImage _blackImage;
//	AllocatedImage _greyImage;
//	AllocatedImage _errorCheckerboardImage;
//
//	VkSampler _defaultSamplerLinear;
//	VkSampler _defaultSamplerNearest;
//
//	MaterialInstance defaultData;
//	GLTFMetallicRoughness metalRoughMaterial;
//
//	Camera mainCamera;
//
//	VulkanEngine& Get();
//	
//	void init();
//
//	void cleanup();
//
//	void draw();
//
//	void drawGeometry(VkCommandBuffer* commandBuffer);
//	void drawBackground(VkCommandBuffer* commandBuffer);
//	void drawImgui(VkCommandBuffer* commandBuffer,  VkImageView targetImageView);
//
//	void updateScene();
//
//	void run();
//
//	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
//
//private:
//
//	void initVulkan();
//	void initSwapchain();
//	void initCommands();
//	void initPipelines();
//	void initDescriptors();
//	void initSyncStructures();
//	void initImgui();
//	void initDefaultData();
//	void initBackgroundPipelines();
//	//void initMeshPipeline();
//	void cleanupVulkan();
//	void cleanupSwapchain();
//	void cleanupCommands();
//	void cleanupPipelines();
//	void cleanupDescriptors();
//	void cleanupSyncStructures();
//	void cleanupImgui();
//	void cleanupDefaultData() {};
//	void cleanupBackgroundPipelines();
//};



struct FrameData {
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;
	
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	DescriptorAllocatorGrowable _frameDescriptors;
	
	ResourceDestructor _frameDestructor;
	void destroy(VulkanDevice* device);
	void init(VulkanDevice* device);
};

class VulkanEngine {
public:
	uint64_t frameCount;
	std::chrono::system_clock::time_point startTime;
	std::chrono::system_clock::time_point previousFrameEnd;
	float deltaTime;
	bool rendering{ false };

	VkExtent2D windowExtent{ 1920 , 1080 };
	struct SDL_Window* window{ nullptr };

	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;
	VulkanDevice vulkanDevice;
	ResourceDestructor mainDestructor;
	VulkanSwapchain swapchain;
	VkSurfaceKHR surface;

	FrameData framesData[FRAMES_IN_FLIGHT];

	Camera camera;

	VkDescriptorPool imguiDescriptorPool;

	DescriptorAllocatorGrowable globalDescriptorAllocator;

	VkDescriptorSet drawImageDescriptors;
	VkDescriptorSetLayout drawImageDescriptorLayout;
	AllocatedImage drawImage;
	
	AllocatedImage depthImage;
	
	VkDescriptorSetLayout singleImageDescriptorLayout;
	
	AllocatedImage whiteImage;
	AllocatedImage blackImage;
	AllocatedImage greyImage;
	AllocatedImage errorCheckerboardImage;
	
	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;

	VulkanPipeline defaultPipeline;

	void init();
	void run();
	void draw();
	void destroy();
private:
	void initVulkan();
	void initSwapchain();
	void initImgui();
	void initPipelines();
	void initDescriptors();
	void initDummyData();

};