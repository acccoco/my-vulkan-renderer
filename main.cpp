#include "./main.h"
#include <set>
#include <sstream>
#include <limits>
#include <cassert>
#include <algorithm>

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


VkSurfaceFormatKHR
Application::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &formats)
{
    for (const auto &format: formats)
    {
        SPDLOG_INFO("format: {}, color space: {}", format.format, format.colorSpace);
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }

    SPDLOG_WARN("no perfect format.");

    // 如果每个都不合格，那么就选择第一个
    return formats[0];
}


VkPresentModeKHR
Application::choose_swap_present_model(const std::vector<VkPresentModeKHR> &present_modes)
{
    for (const auto &present_mode: present_modes)
    {
        SPDLOG_INFO("present mode: {}", present_mode);
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_mode;
    }

    SPDLOG_WARN("no perfect present model.");
    return VK_PRESENT_MODE_FIFO_KHR;
}


VkExtent2D Application::choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities)
{
    /**
     * vulkan 使用的是以 pixel 为单位的分辨率，glfw 初始化窗口时使用的是 screen coordinate 为单位的分辨率
     * 在 Apple Retina display 上，pixel 是 screen coordinate 的 2 倍
     * 最好的方法就是使用 glfwGetFramebufferSize 去查询 window 以 pixel 为单位的大小
     */
    assert(window != nullptr);

    // numeric_limits::max 的大小表示自适应大小
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D acture_extent = {(uint32_t) width, (uint32_t) height};
    acture_extent.width      = std::clamp(acture_extent.width, capabilities.minImageExtent.width,
                                          capabilities.maxImageExtent.width);
    acture_extent.height     = std::clamp(acture_extent.height, capabilities.minImageExtent.height,
                                          capabilities.maxImageExtent.height);
    return acture_extent;
}


void Application::create_swap_chain()
{
    assert(physical_device != VK_NULL_HANDLE);
    assert(logical_device != VK_NULL_HANDLE);
    assert(surface != VK_NULL_HANDLE);

    SwapChainSupportDetail swap_chain_detail = query_swap_chain_support(physical_device);
    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_detail.format_list);
    VkPresentModeKHR present_mode = choose_swap_present_model(swap_chain_detail.present_mode_list);
    swapchain_extent              = choose_swap_extent(swap_chain_detail.capabilities);
    swapchain_iamge_format        = surface_format.format;

    // vulkan 规定，minImageCount 至少是 1
    // maxImageCount 为 0 时表示没有限制
    // TODO 这个是否和双重缓冲有关？
    uint32_t image_cnt = swap_chain_detail.capabilities.minImageCount + 1;
    if (swap_chain_detail.capabilities.maxImageCount > 0 &&
        image_cnt > swap_chain_detail.capabilities.maxImageCount)
        image_cnt = swap_chain_detail.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR create_info = {
            .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface         = surface,
            .minImageCount   = image_cnt,
            .imageFormat     = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent     = swapchain_extent,
            // 有多少个 view，VR 需要多个，普通程序一个就够了
            .imageArrayLayers = 1,
            .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            // 对 swapchain 上的 image 做一些变换，currentTransform 意味着不用任何变换
            .preTransform = swap_chain_detail.capabilities.currentTransform,
            // 是否根据 alpha 与其他 window 进行混合
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode    = present_mode,
            .clipped        = VK_TRUE,    // 多个 windows 之间的颜色模糊

            // 窗口 resize 的时候，之前的 swap chain 会失效，暂不考虑这种情况
            .oldSwapchain = VK_NULL_HANDLE,
    };

    // 当 swap chain 位于 graphics queue family 时，才能对 image 进行绘图
    // 当 swap chain 位于 present queue family 时，才能显示 image
    // 如果 graphics queue family 和 present queue family 是两个不同的 queue family，就需要在
    //  这两个 queue family 之间共享 image
    QueueFamilyIndices indices                = find_queue_families(physical_device);
    uint32_t           queue_family_indices[] = {indices.graphics_family.value(),
                                                 indices.present_family.value()};
    if (indices.graphics_family != indices.present_family)
    {
        create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT,
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices   = queue_family_indices;
    } else
    {
        create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices   = nullptr;
    }

    if (vkCreateSwapchainKHR(logical_device, &create_info, nullptr, &swapchain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain.");

    // 获取 swap chain 里面的 image
    vkGetSwapchainImagesKHR(logical_device, swapchain, &image_cnt, nullptr);
    swapchain_image_list.resize(image_cnt);
    vkGetSwapchainImagesKHR(logical_device, swapchain, &image_cnt, swapchain_image_list.data());
}


Application::SwapChainSupportDetail Application::query_swap_chain_support(VkPhysicalDevice device)
{
    assert(surface != VK_NULL_HANDLE);

    SwapChainSupportDetail details;

    // capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // format
    uint32_t format_cnt;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_cnt, nullptr);
    if (format_cnt != 0)
    {
        details.format_list.resize(format_cnt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_cnt,
                                             details.format_list.data());
    }

    // presentation modes
    uint32_t present_mode_cnt;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_cnt, nullptr);
    if (present_mode_cnt != 0)
    {
        details.present_mode_list.resize(present_mode_cnt);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_cnt,
                                                  details.present_mode_list.data());
    }

    return details;
}


void Application::init_window()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow((int) WIDTH, (int) HEIGHT, "Vulkan", nullptr, nullptr);
}


void Application::show_info()
{
    // vk instance 支持的 extension
    {
        /// 首先获取可用的 extension 数量
        uint32_t ext_cnt = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_cnt, nullptr);
        // 查询 vk 能够支持的扩展的细节
        std::vector<VkExtensionProperties> ext_list(ext_cnt);
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_cnt, ext_list.data());
        // 打印出扩展
        std::stringstream ss;
        ss << "available instance extensions(" << ext_cnt << "): \n";
        for (const auto &ext: ext_list)
            ss << "\t" << ext.extensionName << "\n";
        SPDLOG_INFO("{}", ss.str());
    }
}


void Application::init_vulkan()
{
    if (!check_validation_layer())
        throw std::runtime_error("validation layer required, but not available.");
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swap_chain();
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
            .enabledLayerCount   = (uint32_t) (instance_layer_list.size()),
            .ppEnabledLayerNames = instance_layer_list.data(),

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
    for (const char *layer_needed: instance_layer_list)
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


void Application::create_surface()
{
    assert(instance != VK_NULL_HANDLE);

    // 调用 glfw 来创建 window surface，这样可以避免平台相关的细节
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create widnow surface by glfw.");
}


std::vector<const char *> Application::get_required_ext()
{
    std::vector<const char *> ext_list;

    /// glfw 所需的 vk 扩展，window interface 相关的扩展
    /// VK_KHR_surface: 这个扩展可以暴露出 VkSurfaceKHR 对象，glfw 可以读取这个对象
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
    vkDestroySwapchainKHR(logical_device, swapchain, nullptr);
    vkDestroyDevice(logical_device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);

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


void Application::pick_physical_device()
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
        if (is_physical_device_suitable(device))
        {
            physical_device = device;
            break;
        }
    }
    if (physical_device == VK_NULL_HANDLE)
        throw std::runtime_error("failed to find a suitable GPU.");
}


bool Application::check_device_ext_support(VkPhysicalDevice device)
{
    // 获得设备的所有 extension
    uint32_t ext_cnt;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_cnt, nullptr);
    std::vector<VkExtensionProperties> support_ext_list(ext_cnt);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_cnt, support_ext_list.data());

    // 判断设备是否支持所有的 extension
    std::set<std::string> required_ext_list(device_ext_list.begin(), device_ext_list.end());
    for (const auto &ext: support_ext_list)
        required_ext_list.erase(ext.extensionName);
    return required_ext_list.empty();
}


bool Application::is_physical_device_suitable(VkPhysicalDevice device)
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

    // 检查 extension
    if (!check_device_ext_support(device))
        return false;

    // 检查对 swap chain 的支持
    SwapChainSupportDetail swap_chain_support = query_swap_chain_support(device);
    if (swap_chain_support.format_list.empty() || swap_chain_support.present_mode_list.empty())
        return false;

    return true;
}


Application::QueueFamilyIndices Application::find_queue_families(VkPhysicalDevice device)
{
    assert(surface != VK_NULL_HANDLE);

    // physical device 的 queue family 的数量，以及每个 queue family 的 property
    uint32_t queue_family_cnt = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_cnt, nullptr);
    std::vector<VkQueueFamilyProperties> queue_family(queue_family_cnt);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_cnt, queue_family.data());

    QueueFamilyIndices indices;
    for (int i = 0; i < queue_family_cnt; ++i)
    {
        // 第 i 个 queue family 是否支持 graphics
        if (queue_family[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics_family = i;

        // 第 i 个 queue family 是否支持 present image to window surface
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support)
            indices.present_family = i;
    }

    return indices;
}


void Application::create_logical_device()
{
    assert(physical_device != VK_NULL_HANDLE);    // 创建 logical device 需要 physical device

    // 创建 queue 需要的信息
    QueueFamilyIndices                   indices = find_queue_families(physical_device);
    std::vector<VkDeviceQueueCreateInfo> queue_create_info_list;
    float queue_priority = 1.f;    // 每个队列的优先级，1.f 最高，0.f 最低
    // 可能同一个 queue family 支持多种特性，因此用 set 去重
    for (uint32_t queue_family:
         std::set<uint32_t>{indices.graphics_family.value(), indices.present_family.value()})
        queue_create_info_list.push_back({
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = queue_family,
                .queueCount       = 1,
                .pQueuePriorities = &queue_priority,
        });

    // locgical device 需要的 feature
    VkPhysicalDeviceFeatures device_feature{};

    VkDeviceCreateInfo device_create_info = {
            .sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = (uint32_t) queue_create_info_list.size(),
            .pQueueCreateInfos    = queue_create_info_list.data(),

            /// logical device 也需要有 validation layer
            /// 但是最新的实现已经不需要专门为 logical device 指定 validation layer 了
            .enabledLayerCount   = (uint32_t) instance_layer_list.size(),
            .ppEnabledLayerNames = instance_layer_list.data(),

            .enabledExtensionCount   = (uint32_t) device_ext_list.size(),
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

    // 如果同一个 queue family 既支持 graphics，又支持 present，那么 graphics_queue == present_queue
    vkGetDeviceQueue(logical_device, indices.present_family.value(), 0, &present_queue);
}