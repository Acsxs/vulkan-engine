
#include "vk_engine.h"
#include "vk_initializers.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <VkBootstrap.h>

#include <thread>



#if NDEBUG
constexpr bool bUseValidationLayers = false;
#else
constexpr bool bUseValidationLayers = true;
#endif

using namespace std;

void VulkanEngine::init() {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	
	window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		windowExtent.width,
		windowExtent.height,
		window_flags
	);
	
	SDL_SetRelativeMouseMode(SDL_TRUE);

	initVulkan();
	initSwapchain();
}

void VulkanEngine::run() {
	SDL_Event e;
	bool bQuit = false;
	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0) {
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;

			if (e.type == SDL_WINDOWEVENT) {

				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
					rendering = false;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
					rendering = true;
				}
			}
		}
	
		//do not draw if we are minimized
		if (!rendering) {
			//throttle the speed to avoid endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
	
		draw();
	}
}

void VulkanEngine::draw() {}

void VulkanEngine::destroy() {
	swapchain.destroySwapchain(&vulkanDevice);
	vulkanDevice.destroy();
	vkb::destroy_debug_utils_messenger(instance, debugMessenger, nullptr);
	vkDestroyInstance(instance, nullptr);

}



void VulkanEngine::initVulkan() {
	vkb::InstanceBuilder vkbInstanceBuilder;

	vkb::Instance vkbInstance = vkbInstanceBuilder
		.set_app_name("Iteration 1")
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build()
		.value();

	instance = vkbInstance.instance;
	debugMessenger = vkbInstance.debug_messenger;

	SDL_Vulkan_CreateSurface(window, instance, &surface);

	VkPhysicalDeviceVulkan13Features physicalDeviceFeatures13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	physicalDeviceFeatures13.dynamicRendering = true;
	physicalDeviceFeatures13.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features physicalDeviceFeatures12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	physicalDeviceFeatures12.bufferDeviceAddress = true;
	physicalDeviceFeatures12.descriptorIndexing = true;

	vkb::PhysicalDeviceSelector vkbPhysicalDeviceSelector{ vkbInstance };
	vkb::PhysicalDevice vkbPhysicalDevice = vkbPhysicalDeviceSelector
		.set_minimum_version(1, 3)
		.set_required_features_13(physicalDeviceFeatures13)
		.set_required_features_12(physicalDeviceFeatures12)
		.set_surface(surface)
		.select()
		.value();

	vulkanDevice.init(vkbPhysicalDevice, instance);
}

void VulkanEngine::initSwapchain() {
	swapchain.createSwapchain(&vulkanDevice, surface, windowExtent.width, windowExtent.height);
	VkExtent3D drawImageExtent = {
		windowExtent.width,
		windowExtent.height,
		1
	};
}
