/**
* 关于 validation layer
* 开启 validation layer 之前需要先启用 VK_EXT_DEBUG_UTILS_EXTENSION_NAME 扩展
* 还需要在 instance 的创建中指定 "VK_LAYER_KHRONOS_validation" layer
* 使用 validation layer 之后，可以支持 debug messenger
*/

#pragma once
#include <vector>
#include <cstdlib>
#include <optional>
#include <iostream>
#include <stdexcept>

#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_beta.h>    // 在 metal 上运行 vulkan，需要这个头文件
#include <spdlog/spdlog.h>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


void init_spdlog();


class Application
{
public:
    void run()
    {
        init_window();
        init_vulkan();
        mainLoop();
        cleanup();
    }

private:
    // 窗口对象，需要手动释放资源
    GLFWwindow    *window{};
    const uint32_t WIDTH  = 800;
    const uint32_t HEIGHT = 600;
    // 整个应用程序的 vulkan instance，需要手动释放资源
    VkInstance instance{VK_NULL_HANDLE};

    // 需要用到的 layer
    const std::vector<const char *> needed_layer_list = {
            "VK_LAYER_KHRONOS_validation",    // validation layer
    };

    // debug messenger 的 handle，需要最后手动释放资源
    VkDebugUtilsMessengerEXT debug_messenger;

    // 物理设备的 handle，在销毁 instance 时会自动销毁，无需手动释放资源
    VkPhysicalDevice physical_device{VK_NULL_HANDLE};

    // logical device 的 handle，需要手动释放资源
    VkDevice logical_device;

    // graphics command 相关的 queue 的 handle，会跟随 logical device 一起销毁
    VkQueue graphics_queue;

    // debug messenger 相关的配置信息
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            // 会触发 callback 的严重级别
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            // 会触发 callback 的信息类型
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback,    // 回调函数
            .pUserData       = nullptr,
    };

    // index of queue family in one physical device
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;    // 支持 VK_QUEUE_GRAPHICS_BIT 的 queue family

        bool is_complete() { return graphics_family.has_value(); }
    };

    /**
     * 通过 glfw 来初始化 window
     */
    void init_window();

    void init_vulkan();

    /**
     * 创建一个 vk 实例，这个实例将 vk 的库/驱动和这个应用连接在一起
     */
    void create_instance();

    /**
     * 检查需要用到的 validation layers 是否受支持
     */
    bool check_validation_layer();

    /**
     * 获得当前应用所需的扩展
     */
    [[nodiscard]] static std::vector<const char *> get_required_ext();

    /**
     * 为 instance 设置用于 debug 的 messenger，只有开启了 validation layer 才能用这个
     */
    void setup_debug_messenger();

    /**
     * 用于 debug 的回调函数
     * @param message_severity 严重程度 verbose, info, warning, error
     * @param message_type 信息的类型 general, validation, perfomance
     * @param callback_data 信息的关键内容
     * @param user_data 自定义的数据
     * @return 是否应该 abort
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                   VkDebugUtilsMessageTypeFlagsEXT             message_type,
                   const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

    /**
     * 选择一个 physical device 来使用
     */
    void pick_gpu();

    /**
     * 检查一个 physical device 释放适合当前应用
     * 会检查 device 支持的 feature，检查其支持的 queue family
     */
    static bool is_gpu_suitable(VkPhysicalDevice device);

    /**
     * 找到 physical device 支持的 queue families
     * 不同的 queue families 支持不同类型的 command
     * 例如：一些只支持 compute command，另一些只支持 transfer command
     * @return 某个 queue family 在 device 的 queue family 列表的 index
     */
    static QueueFamilyIndices find_queue_families(VkPhysicalDevice device);

    void create_logical_device();

    void mainLoop();

    void cleanup();
};