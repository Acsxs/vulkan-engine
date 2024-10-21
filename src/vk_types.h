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

#include "vk_mem_alloc.h"


#define VK_CHECK(x) do { VkResult err = x; if (err) {fmt::println("Detected Vulkan error: {}", string_VkResult(err)); abort(); } } while (0)

constexpr unsigned int FRAMES_IN_FLIGHT = 3;
