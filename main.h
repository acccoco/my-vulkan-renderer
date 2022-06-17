/**
* 关于 validation layer
* 开启 validation layer 之前需要先启用 VK_EXT_DEBUG_UTILS_EXTENSION_NAME 扩展
* 还需要在 instance 的创建中指定 "VK_LAYER_KHRONOS_validation" layer
* 使用 validation layer 之后，可以支持 debug messenger
*/

#pragma once
#include <vector>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>


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
    GLFWwindow    *window{};
    const uint32_t WIDTH  = 800;
    const uint32_t HEIGHT = 600;
    VkInstance     instance{};  // 整个应用程序的 vulkan instance

    // 需要用到的 layer
    const std::vector<const char *> needed_layer_list = {
            "VK_LAYER_KHRONOS_validation",  // validation layer
    };

    // 用于 debug 的 messenger
    VkDebugUtilsMessengerEXT debug_messenger;

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
    [[nodiscard]] static std::vector<const char *> get_required_ext() ;

    /**
     * 为 instance 设置用于 debug 的 messenger，只有开启了 validation layer 才能用这个
     */
    void setup_debug_messenger();

    /**
     * 创建一个 create info，它用于创建 DebugUtilsMessenger
     */
    static VkDebugUtilsMessengerCreateInfoEXT gen_debug_messenger_create_info();

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

    void mainLoop();

    void cleanup();
};