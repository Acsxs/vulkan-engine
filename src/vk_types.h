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

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "vk_resources.h"
#include "vk_material.h"

#define VK_CHECK(x) do { VkResult err = x; if (err) {fmt::println("Detected Vulkan error: {}", string_VkResult(err)); abort(); } } while (0)

constexpr unsigned int FRAMES_IN_FLIGHT = 3;



struct Vertex {

    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};


struct DrawObjectInfo {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    VkDeviceAddress vertexBufferAddress;
    glm::mat4 transform;

    MaterialReference* material;
};


struct DrawObjectCollection {
    std::vector<DrawObjectInfo> opaqueObjects;
    std::vector<DrawObjectInfo> transparentObjects;
};


// base class for a renderable dynamic object
class IRenderable {
    virtual void draw(const glm::mat4& topMatrix, DrawObjectCollection& ctx) = 0;
};

