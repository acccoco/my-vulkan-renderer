#pragma once

#include <spdlog/spdlog.h>

#define VK_ENABLE_BETA_EXTENSIONS    // vulkan_beta.h，在 metal 上运行 vulkan，需要这个
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_UNION_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

// 需要在 vulkan 之后被 include，内部的一些声明需要来自 vulkan 的 macro
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>