
#include "./main.h"
#include <sstream>
#include <cassert>

int main()
{
    init_spdlog();

    Application app{};

    try
    {
        app.run();
    } catch (const std::exception &e)
    {
        SPDLOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


void init_spdlog()
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%^%L%$]%v");
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
    pick_gpu();
    create_logical_device();
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
        std::stringstream ss;
        ss << "available extensions(" << ext_cnt << "): \n";
        for (const auto &ext: ext_list)
            ss << "\t" << ext.extensionName << "\n";
        SPDLOG_INFO("{}", ss.str());
    }

    // 这些数据用于指定当前应用程序需要的全局 extension 以及 validation layers
    VkInstanceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,

            /// 一般情况下 debug messenger 只有在 instance 创建后和销毁前才是有效的，
            ///     将这个 create info 传递给 instance 创建信息的 pNext 字段，
            ///     可以在 instance 的创建和销毁过程进行 debug
            .pNext = &debug_messenger_create_info,

            /// 表示 vulkan 除了枚举出默认的 physical device 外，
            /// 还会枚举出符合 vulkan 可移植性的 physical device
            /// 基于 metal 的 vulkan 需要这个
            .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,

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
    std::stringstream log_ss;
    log_ss << "available layers(" << layer_cnt << "):\n";
    for (const auto &layer: supported_layer_list)
        log_ss << "\t" << layer.layerName << ": " << layer.description << "\n";
    SPDLOG_INFO("{}", log_ss.str());

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
    ext_list.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // 基于 metal API 的 vulkan 实现需要这些扩展
    ext_list.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    ext_list.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    // 所有的扩展
    std::stringstream log_ss;
    log_ss << "application require extensions(" << ext_list.size() << "): \n";
    for (const char *ext: ext_list)
        log_ss << "\t" << ext << "\n";
    SPDLOG_INFO("{}", log_ss.str());
    return ext_list;
}


void Application::setup_debug_messenger()
{
    // 这个函数需要在 instance 已经创建之后执行
    assert(instance != VK_NULL_HANDLE);

    // 因为 vkCreateDebugUtilsMessengerEXT 是扩展函数，因此需要查询函数指针
    auto vkCreateDebugUtilsMessengerEXT =
            (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
                    instance, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT == nullptr)
        throw std::runtime_error("failed to find function(vkCreateDebugUtilsMessengerEXT)");

    // 为 instance 设置 debug messenger
    if (vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, nullptr,
                                       &debug_messenger) != VK_SUCCESS)
        throw std::runtime_error("failed to setup debug messenger.");
}


VkBool32 Application::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                     VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                     const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                     void *)
{

    const char *type;
    switch (message_type)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: type = "Gene"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: type = "Vali"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: type = "Perf"; break;
        default: type = "?";
    }

    switch (message_severity)
    {
#define _TEMP_LOG_(logger) logger("[validation layer][{}]: {}", type, callback_data->pMessage)
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: _TEMP_LOG_(SPDLOG_WARN); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: _TEMP_LOG_(SPDLOG_ERROR); break;
        default: _TEMP_LOG_(SPDLOG_INFO);
#undef _TEMP_LOG_
    }

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
    vkDestroyDevice(logical_device, nullptr);

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


void Application::pick_gpu()
{
    // 这个函数需要在 instance 创建之后执行
    assert(instance != VK_NULL_HANDLE);

    // 获得 physical device 的列表
    uint32_t device_cnt = 0;
    vkEnumeratePhysicalDevices(instance, &device_cnt, nullptr);
    if (device_cnt == 0)
        throw std::runtime_error("no physical device found with vulkan support.");
    std::vector<VkPhysicalDevice> device_list(device_cnt);
    vkEnumeratePhysicalDevices(instance, &device_cnt, device_list.data());
    SPDLOG_INFO("physical device with vulkan support:({})", device_cnt);

    // 检查 physical device 是否符合要求，只需要其中一块就够了
    for (const auto &device: device_list)
    {
        if (is_gpu_suitable(device))
        {
            physical_device = device;
            break;
        }
    }
    if (physical_device == VK_NULL_HANDLE)
        throw std::runtime_error("failed to find a suitable GPU.");
}


bool Application::is_gpu_suitable(VkPhysicalDevice device)
{
    // 检查 physical device 的 properties 和 feature
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures   device_features;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);
    {
        std::stringstream ss;
        ss << "physical deivice info: \n";
        ss << "\t name: " << device_properties.deviceName << "\n";
        ss << "\t type: " << device_properties.deviceType
           << "(0:other, 1:integrated-gpu, 2:discretegpu, 3:virtual-gpu, 4:cpu)\n";
        /// Mac M1 并不支持 geometry shader，实际上，geometry shader 效率很低
        /// 应该使用 compute shader 来替代
        /// https://forum.unity.com/threads/geometry-shader-on-mac.1056659/
        ss << "\t geometry shader(bool): " << device_features.geometryShader << "\n";
        ss << "\t tessellation shader(bool): " << device_features.tessellationShader << "\n";
        SPDLOG_INFO("{}", ss.str());
    }
    if (!device_features.tessellationShader)
        return false;

    // 检查 physical device 的 queue family
    QueueFamilyIndices indices = find_queue_families(device);
    if (!indices.is_complete())
        return false;
    SPDLOG_INFO("queue family is supported.");

    return true;
}


Application::QueueFamilyIndices Application::find_queue_families(VkPhysicalDevice device)
{
    uint32_t queue_family_cnt = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_cnt, nullptr);

    std::vector<VkQueueFamilyProperties> queue_family(queue_family_cnt);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_cnt, queue_family.data());

    QueueFamilyIndices indices;
    for (int i = 0; i < queue_family_cnt; ++i)
    {
        if (queue_family[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics_family = i;
    }

    return indices;
}


void Application::create_logical_device()
{
    // 创建 logical device 需要 physical device
    assert(physical_device != VK_NULL_HANDLE);

    // 创建 queue 需要的信息
    QueueFamilyIndices indices = find_queue_families(physical_device);
    float queue_priority[]     = {1.0f};    // 每个队列的优先级，1.f 最高，0.f 最低
    VkDeviceQueueCreateInfo queue_create_info = {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = indices.graphics_family.value(),
            .queueCount       = 1,
            .pQueuePriorities = queue_priority,    // 指定每个队列的优先级
    };

    // locgical device 需要的 feature
    VkPhysicalDeviceFeatures device_feature{};
    std::vector<const char*> device_ext_list = {
            // 这是一个临时的扩展（vulkan_beta.h)，在 metal API 上模拟 vulkan 需要这个扩展
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
    };

    VkDeviceCreateInfo device_create_info = {
            .sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos    = &queue_create_info,

            /// logical device 也需要有 validation layer
            /// 但是最新的实现已经不需要专门为 logical device 指定 validation layer 了
            .enabledLayerCount   = (uint32_t) needed_layer_list.size(),
            .ppEnabledLayerNames = needed_layer_list.data(),

            .enabledExtensionCount = (uint32_t) device_ext_list.size(),
            .ppEnabledExtensionNames = device_ext_list.data(),

            .pEnabledFeatures = &device_feature,    // 指定 logical device 的 features
    };

    if (vkCreateDevice(physical_device, &device_create_info, nullptr, &logical_device) !=
        VK_SUCCESS)
        throw std::runtime_error("failed to create logical device.");

    // 取得 logical device 的各个 queue 的 handle
    // 第二个参数表示 queue family
    // 第三个参数表示该 queue family 中 queue 的 index，
    //  因为只创建了一个 graphics queue family 的 queue，因此 index = 0
    vkGetDeviceQueue(logical_device, indices.graphics_family.value(), 0, &graphics_queue);
}