#pragma once
#include <iostream>
#include "include_vk.hpp"
#include "instance.hpp"
#include "global.hpp"
#include "window.hpp"


// TODO 不使用 static，但是可以使用 singleton。这样依赖关系明确一些。

namespace Hiss
{
class ApplicationBase
{
private:
    struct DebugUserData
    {
        std::shared_ptr<spdlog::logger> _logger;
    };


#ifdef NDEBUG    // 检测 debug 模式，决定是否开启 validation
    static constexpr bool debug = false;
#else
    static constexpr bool debug = true;
#endif


    const int32_t WINDOW_INIT_WIDTH  = 800;
    const int32_t WINDOW_INIT_HEIGHT = 800;

    std::shared_ptr<spdlog::logger>      _logger;
    DebugUserData                        _debug_user_data;
    std::unique_ptr<Hiss::Window>        _window;
    vk::DebugUtilsMessengerCreateInfoEXT _debug_msger_info;
    std::unique_ptr<Instance>            _instance;
    vk::DebugUtilsMessengerEXT           _debug_msger;
    vk::SurfaceKHR                       _surface;


    std::vector<vk::ShaderModule> shader_modules;    // 最后一起销毁


    void logger_init();
    void debug_msger_info_create();
    void instance_init(const std::string &app_name);
    void debug_utils_init();


    static vk::Bool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                     VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                     const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);


protected:
    vk::PipelineShaderStageCreateInfo shader_load(const std::string &file, vk::ShaderStageFlagBits stage);


public:
    ApplicationBase(const std::string &app_name);
    ~ApplicationBase();
    virtual void prepare() {}
    virtual void draw() {}
    virtual void update() {}
    virtual void run() {}
};
}    // namespace Hiss


#define APP_RUN(AppClass)                                                                                              \
    int main()                                                                                                         \
    {                                                                                                                  \
        try                                                                                                            \
        {                                                                                                              \
            AppClass app;                                                                                              \
            app.run();                                                                                                 \
        }                                                                                                              \
        catch (const std::exception &e)                                                                                \
        {                                                                                                              \
            std::cerr << "exception: " << e.what() << std::endl;                                                       \
            return EXIT_FAILURE;                                                                                       \
        }                                                                                                              \
        return EXIT_SUCCESS;                                                                                           \
    }
