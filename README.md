# Vulkan Engine Implementation

## How to run
1. If not already installed, install  Visual Studio from [Microsoft](https://visualstudio.microsoft.com/downloads/), make sure to install the C++ module
2. If not already installed, install Vulkan SDK from [LunarG](https://vulkan.lunarg.com/sdk/home)
3. If not already installed, install CMake from [CMake](https://cmake.org/download/)
4. Open CMake (cmake-gui), navigate source code to project location, make sure it is the project folder and not the src folder
5. Click "Configure", then "Generate", then "Open project". This should open Visual Studio
6. Right click on engine project in solution explorer, then click "Set as Startup Project"
7. Run the local windows debugger

## Features
- Basic gLTF file loading
- Texture support
- Materials support
- Normal map support
- Simple Physically Based Rendering (PBR) lighting model
- Camera

## Structure

