#pragma once
#include "include_vk.hpp"


class LogStatic
{
private:
    inline static std::shared_ptr<spdlog::logger> _logger{nullptr};
    inline static std::shared_ptr<spdlog::logger> _val_logger{nullptr};

public:
    static void init()
    {
        assert(_logger == nullptr && _val_logger == nullptr);

        _logger = spdlog::stdout_color_st("logger");
        _logger->set_level(spdlog::level::trace);
        _logger->set_pattern("[%^%L%$] %v");

        _val_logger = spdlog::stdout_color_st("validation");
        _val_logger->set_level(spdlog::level::trace);
        _val_logger->set_pattern("[%^%n%$] %v");
    }

    static auto logger()
    {
        assert(_logger != nullptr);
        return _logger;
    }

    static auto val_logger()
    {
        assert(_val_logger != nullptr);
        return _val_logger;
    }
};


class WindowStatic
{
private:
    WindowStatic() = default;

    struct UserData {
        bool resized;
    };
    inline static GLFWwindow *_window = nullptr;
    inline static UserData _user_data{false};


public:
    static void init(int width, int height)
    {
        assert(_window == nullptr);

        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        _window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);


        /* 设置窗口大小改变的 callback */
        glfwSetWindowUserPointer(_window, &_user_data);
        glfwSetFramebufferSizeCallback(_window, [](GLFWwindow *window, int width, int height) {
            auto user_data     = reinterpret_cast<UserData *>(glfwGetWindowUserPointer(window));
            user_data->resized = true;
        });
    }

    static GLFWwindow *window_get()
    {
        assert(_window != nullptr);
        return _window;
    }

    static void close()
    {
        glfwDestroyWindow(_window);
        glfwTerminate();
    }

    /* 查看 window 是否以及 resize 了 */
    static bool resized() { return _user_data.resized; }
    /* 设置 window 是否 resize */
    static void resized(bool state) { _user_data.resized = state; }

    /**
     * 等待退出最小化。等待时阻塞
     */
    static void wait_exit_minimize()
    {
        LogStatic::logger()->info("check window minimize.");


        int width = 0, height = 0;
        glfwGetFramebufferSize(_window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwWaitEvents();
            glfwGetFramebufferSize(_window, &width, &height);
        }


        LogStatic::logger()->info("window is not minimize.");
    }
};


bool instance_layers_check(const std::vector<const char *> &layers);

vk::Instance instance_create(const vk::DebugUtilsMessengerCreateInfoEXT &dbg_msger_create_info);


class DebugUtils
{
private:
    inline static vk::DebugUtilsMessengerEXT _dbg_msger;


public:
    static void msger_init(const vk::Instance &instance)
    {
        _dbg_msger = instance.createDebugUtilsMessengerEXT(DebugUtils::dbg_msg_info);
    }

    static void msger_free(const vk::Instance &instance) { instance.destroy(_dbg_msger); }


    static vk::Bool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                     VkDebugUtilsMessageTypeFlagsEXT message_type,
                                     const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                     void *)
    {
        const char *type;
        switch (static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(message_type))
        {
            case vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral: type = "General"; break;
            case vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation: type = "Validation"; break;
            case vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance: type = "Performance"; break;
            default: type = "?";
        }

        auto val_logger = LogStatic::val_logger();
        switch (message_severity)
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                val_logger->warn("[{}]: {}", type, callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                val_logger->error("[{}]: {}", type, callback_data->pMessage);
                break;
            default: val_logger->info("[{}]: {}", type, callback_data->pMessage);
        }

        return VK_FALSE;
    }


    inline static const vk::DebugUtilsMessengerCreateInfoEXT dbg_msg_info = {
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = DebugUtils::debug_callback,
            .pUserData       = nullptr,
    };
};