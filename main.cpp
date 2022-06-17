
#include "./main.h"

int main()
{
    Application app{};

    try
    {
        app.run();
    } catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


void Application::init_window()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow((int) WIDTH, (int) HEIGHT, "Vulkan", nullptr, nullptr);
}


void Application::init_vulkan()
{
    if (!check_validation_layer())
        throw std::runtime_error("validation layer required, but not available.");
    create_instance();
    setup_debug_messenger();
}


void Application::create_instance()

{
    // （这是可选的） 关于应用程序的信息，驱动可以根据这些信息对应用程序进行一些优化
    VkApplicationInfo app_info = {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,    // 这个结构体的类型
            .pApplicationName   = "vk app",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_0,
    };

    // 所需的 vk 扩展
    std::vector<const char *> required_extensions = get_required_ext();

    // vk 驱动支持的扩展
    {
        /// 首先获取可用的 extension 数量
        uint32_t ext_cnt = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_cnt, nullptr);
        // 查询 vk 能够支持的扩展的细节
        std::vector<VkExtensionProperties> ext_list(ext_cnt);
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_cnt, ext_list.data());
        // 打印出扩展
        std::cout << "available extensions(" << ext_cnt << "):\n";
        for (const auto &ext: ext_list)
            std::cout << '\t' << ext.extensionName << '\n';
    }

    /// 一般情况下 debug messenger 只有在 instance 创建后和销毁前才是有效的，
    ///     将这个 create info 传递给 instance 创建信息的 pNext 字段，
    ///     可以在 instance 的创建和销毁过程进行 debug
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info =
            gen_debug_messenger_create_info();

    // 这些数据用于指定当前应用程序需要的全局 extension 以及 validation layers
    VkInstanceCreateInfo create_info = {
            .sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext            = &debug_messenger_create_info,
            .pApplicationInfo = &app_info,
            // layers
            .enabledLayerCount   = (uint32_t) (needed_layer_list.size()),
            .ppEnabledLayerNames = needed_layer_list.data(),
            // extensions
            .enabledExtensionCount   = (uint32_t) required_extensions.size(),
            .ppEnabledExtensionNames = required_extensions.data(),
    };

    // 创建 vk 的 instance
    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create vk instance.");
}


bool Application::check_validation_layer()

{
    // layers 的数量
    uint32_t layer_cnt = 0;
    vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr);

    // 当前系统支持的 layer
    std::vector<VkLayerProperties> supported_layer_list(layer_cnt);
    vkEnumerateInstanceLayerProperties(&layer_cnt, supported_layer_list.data());
    std::cout << "available layers(" << layer_cnt << "):\n";
    for (const auto &layer: supported_layer_list)
        std::cout << '\t' << layer.layerName << ": " << layer.description << '\n';

    // 检查需要的 layer 是否受支持（笛卡尔积操作）
    for (const char *layer_needed: needed_layer_list)
    {
        bool layer_found = false;
        for (const auto &layer_supported: supported_layer_list)
        {
            if (strcmp(layer_needed, layer_supported.layerName) == 0)
            {
                layer_found = true;
                break;
            }
        }
        if (!layer_found)
            return false;
    }
    return true;
}


std::vector<const char *> Application::get_required_ext()
{
    std::vector<const char *> ext_list;

    // glfw 所需的 vk 扩展
    uint32_t     glfw_ext_cnt = 0;
    const char **glfw_ext_list;
    glfw_ext_list = glfwGetRequiredInstanceExtensions(&glfw_ext_cnt);
    for (int i = 0; i < glfw_ext_cnt; ++i)
        ext_list.push_back(glfw_ext_list[i]);

    // validation layer 需要的扩展
    const char *validation_ext_name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    ext_list.push_back(validation_ext_name);


    // 所有的扩展
    std::cout << "application require extensions(" << ext_list.size() << "):\n";
    for (const char *ext: ext_list)
        std::cout << '\t' << ext << '\n';
    return ext_list;
}


VkDebugUtilsMessengerCreateInfoEXT Application::gen_debug_messenger_create_info()
{
    return VkDebugUtilsMessengerCreateInfoEXT{
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
}


void Application::setup_debug_messenger()
{
    VkDebugUtilsMessengerCreateInfoEXT create_info = gen_debug_messenger_create_info();

    // 因为 vkCreateDebugUtilsMessengerEXT 是扩展函数，因此需要查询函数指针
    auto vkCreateDebugUtilsMessengerEXT =
            (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
                    instance, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT == nullptr)
        throw std::runtime_error("failed to find function(vkCreateDebugUtilsMessengerEXT)");

    // 为 instance 设置 debug messenger
    if (vkCreateDebugUtilsMessengerEXT(instance, &create_info, nullptr, &debug_messenger) !=
        VK_SUCCESS)
        throw std::runtime_error("failed to setup debug messenger.");
}


VkBool32 Application::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                     VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                     const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                     void *)
{
    const char *severity;
    switch (message_severity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severity = "V"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: severity = "I"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severity = "W"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: severity = "E"; break;
        default: severity = "?";
    }
    const char *type;
    switch (message_type)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: type = "Gene"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: type = "Vali"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: type = "Perf"; break;
        default: type = "?";
    }

    std::cerr << "validation layer[" << severity << "][" << type << "]: " << callback_data->pMessage
              << std::endl;
    return VK_FALSE;
}


void Application::mainLoop()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    }
}


void Application::cleanup()
{
    // vk debug messenger，需要在 instance 之前销毁
    auto vkDestroyDebugUtilsMessengerEXT =
            (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
                    instance, "vkDestroyDebugUtilsMessengerEXT");
    if (vkDestroyDebugUtilsMessengerEXT != nullptr)
        vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);

    // vk instance
    vkDestroyInstance(instance, nullptr);

    // glfw
    glfwDestroyWindow(window);
    glfwTerminate();
}