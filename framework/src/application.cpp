#include "../application.hpp"
#include "../tools.hpp"
#include "../vk_common.hpp"


/**
 * vulkan 相关函数的 dynamic loader
 * 这是变量定义的位置，vulkan.hpp 会通过 extern 引用这个全局变量
 */
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;


vk::PipelineShaderStageCreateInfo Hiss::ApplicationBase::shader_load(const std::string      &file,
                                                                     vk::ShaderStageFlagBits stage)
{
    vk::Device d;    // FIXME

    std::vector<char>          code = read_file(file);
    vk::ShaderModuleCreateInfo info = {
            .codeSize = code.size(),    // 单位是字节

            /* vector 容器可以保证转换为 uint32 后仍然是对齐的 */
            .pCode = reinterpret_cast<const uint32_t *>(code.data()),
    };
    vk::ShaderModule shader_module = d.createShaderModule(info);
    shader_modules.push_back(shader_module);

    return vk::PipelineShaderStageCreateInfo{
            .stage  = stage,
            .module = shader_module,
            .pName  = "main",
    };
}


Hiss::ApplicationBase::~ApplicationBase()
{
    vk::Device d;    // FIXME


    for (auto shader_module: shader_modules)
        d.destroy(shader_module);
}


Hiss::ApplicationBase::ApplicationBase(const std::string &app_name)
{
    logger_init();
    debug_msger_info_create();
    instance_init(app_name);
    _debug_msger = _instance->handle_get().createDebugUtilsMessengerEXT(_debug_msger_info);
    _window      = std::make_unique<Window>(app_name, WINDOW_INIT_WIDTH, WINDOW_INIT_HEIGHT);
    _surface     = _window->surface_create(_instance->handle_get());
}


void Hiss::ApplicationBase::instance_init(const std::string &app_name)
{
    /* 找到所需的 extensions */
    std::vector<const char *> extensions = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,    // validation layer 需要的扩展

            /* 基于 metal API 的 vulkan 实现需要这些扩展 */
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };
    auto glfw_extension = WindowStatic::extensions_get();
    extensions.insert(extensions.end(), glfw_extension.begin(), glfw_extension.end());


    /* 所需的 layers */
    std::vector<const char *> layers = {
            "VK_LAYER_KHRONOS_validation",
    };
    if (Hiss::instance_layers_check(layers))
        throw std::runtime_error("layers unsupported.");


    /**
     * 在 instance 之前，需要初始化 loader 找到一些函数
     * 在 instance 之后，还需要初始化 loader，找到和 instance 相关的函数
     */
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    _instance = std::make_unique<Instance>(app_name, extensions, layers, _debug_msger_info);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance->handle_get());
}


void Hiss::ApplicationBase::logger_init()
{
    _logger = spdlog::stdout_color_st("logger");
    _logger->set_level(spdlog::level::trace);
    _logger->set_pattern("[%^%L%$] %v");

    _debug_user_data._logger = spdlog::stdout_color_st("validation");
    _debug_user_data._logger->set_level(spdlog::level::trace);
    _debug_user_data._logger->set_pattern("[%^%n%$] %v");
}


vk::Bool32 Hiss::ApplicationBase::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                                 VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                                 const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                                 void                                       *user_data)
{
    const char *type;
    switch (static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(message_type))
    {
        case vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral: type = "General"; break;
        case vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation: type = "Validation"; break;
        case vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance: type = "Performance"; break;
        default: type = "?";
    }

    auto logger = reinterpret_cast<DebugUserData *>(user_data)->_logger;
    switch (message_severity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            logger->warn("[{}]: {}", type, callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            logger->error("[{}]: {}", type, callback_data->pMessage);
            break;
        default: logger->info("[{}]: {}", type, callback_data->pMessage);
    }

    return VK_FALSE;
}


void Hiss::ApplicationBase::debug_utils_init()
{
    _debug_msger_info = {
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debug_callback,
            .pUserData       = &_debug_user_data,
    };
}


void Hiss::ApplicationBase::debug_msger_info_create()
{
    _debug_msger_info = {
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debug_callback,
            .pUserData       = &_debug_user_data,
    };
}
