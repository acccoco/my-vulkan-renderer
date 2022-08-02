#pragma once


#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>


#define VK_ENABLE_BETA_EXTENSIONS    // vulkan_beta.h，在 metal 上运行 vulkan，需要这个
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_UNION_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>


/* 需要在 vulkan 之后被 include，内部的一些声明需要来自 vulkan 的 macro */
#include <GLFW/glfw3.h>


#define GLM_FORCE_RADIANS
/* 透视投影矩阵生成的深度范围是 [-1.0, 1.0]，这里可以将其改成 [0.0, 1.0] */
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
/* 让 glm 的类型支持 std::hash<...>() */
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>


#include <stb_image.h>


#define BITS_CONTAIN(bits_a, bits_b) ((bits_a & bits_b) == bits_b)
