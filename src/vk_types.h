// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <fmt/core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <iostream>


#define VK_CHECK(x) do { VkResult err = x; if (err) {fmt::println("Detected Vulkan error: {}", string_VkResult(err)); abort(); } } while (0)

constexpr unsigned int FRAMES_IN_FLIGHT = 3;

struct MaterialInstance;

struct Vertex {

    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct DrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct DrawObjectInfo {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    VkDeviceAddress vertexBufferAddress;
    glm::mat4 transform;

    MaterialInstance* material;
};


struct DrawObjectCollection {
    std::vector<DrawObjectInfo> opaqueObjects;
    std::vector<DrawObjectInfo> transparentObjects;
};


