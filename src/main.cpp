#include "vk_engine.h"

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.destroy();	

	return 0;
}
