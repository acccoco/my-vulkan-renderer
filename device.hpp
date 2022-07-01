#pragma once

#include <vector>
#include <optional>


#include "./include_vk.hpp"



struct DeviceInfo {
    vk::PhysicalDeviceProperties properties;
    vk::PhysicalDeviceFeatures features;
    vk::PhysicalDeviceMemoryProperties mem_properties;

    std::vector<vk::QueueFamilyProperties> queue_family_property_list;
    std::optional<uint32_t> graphics_queue_family_idx;
    std::optional<uint32_t> present_queue_family_idx;

    std::vector<vk::ExtensionProperties> support_ext_list;


    inline static DeviceInfo get_info(const vk::PhysicalDevice &physical_device, const vk::SurfaceKHR &surface)
    {
        DeviceInfo info{
                .properties                 = physical_device.getProperties(),
                .features                   = physical_device.getFeatures(),
                .mem_properties             = physical_device.getMemoryProperties(),
                .queue_family_property_list = physical_device.getQueueFamilyProperties(),
                .support_ext_list           = physical_device.enumerateDeviceExtensionProperties(),
        };

        for (uint32_t i = 0; i < info.queue_family_property_list.size(); ++i)
        {
            if (info.queue_family_property_list[i].queueFlags & vk::QueueFlagBits::eGraphics)
                info.graphics_queue_family_idx = i;
            if (physical_device.getSurfaceSupportKHR(i, surface))
                info.present_queue_family_idx = i;
        }

        return info;
    }
};


struct SurfaceInfo {
    vk::SurfaceCapabilitiesKHR capability;
    std::vector<vk::SurfaceFormatKHR> format_list;
    std::vector<vk::PresentModeKHR> present_mode_list;

    vk::SurfaceFormatKHR format;
    vk::PresentModeKHR present_mode;
    vk::Extent2D extent;


    static SurfaceInfo get_info(const vk::PhysicalDevice &physical_device, const vk::SurfaceKHR &surface,
                                GLFWwindow *window)
    {
        SurfaceInfo info = {
                .capability        = physical_device.getSurfaceCapabilitiesKHR(surface),
                .format_list       = physical_device.getSurfaceFormatsKHR(surface),
                .present_mode_list = physical_device.getSurfacePresentModesKHR(surface),
        };


        info.format       = choose_format(info.format_list);
        info.present_mode = choose_present_mode(info.present_mode_list);
        info.extent       = choose_extent(info.capability, window);

        return info;
    }


    static vk::SurfaceFormatKHR choose_format(const std::vector<vk::SurfaceFormatKHR> &format_list_)
    {
        for (const auto &format: format_list_)
            if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return format;
        return format_list_[0];
    }


    static vk::PresentModeKHR choose_present_mode(const std::vector<vk::PresentModeKHR> &present_mode_list_)
    {
        for (const auto &present_mode: present_mode_list_)
            if (present_mode == vk::PresentModeKHR::eMailbox)
                return present_mode;
        return vk::PresentModeKHR::eFifo;
    }


    static vk::Extent2D choose_extent(vk::SurfaceCapabilitiesKHR &capability_, GLFWwindow *window)
    {
        /**
         * vulkan 使用的是以 pixel 为单位的分辨率，glfw 初始化窗口时使用的是 screen coordinate 为单位的分辨率
         * 在 Apple Retina display 上，pixel 是 screen coordinate 的 2 倍
         * 最好的方法就是使用 glfwGetFramebufferSize 去查询 window 以 pixel 为单位的大小
         */

        if (capability_.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return capability_.currentExtent;


        int width, height;
        glfwGetFramebufferSize(window, &width, &height);


        vk::Extent2D extent;
        extent.width = std::clamp((uint32_t) width, capability_.minImageExtent.width, capability_.maxImageExtent.width);
        extent.height =
                std::clamp((uint32_t) height, capability_.minImageExtent.height, capability_.maxImageExtent.height);
        return extent;
    }
};


struct GLFWUserData {
    bool framebuffer_resized;
};


/**
 * 初始化 glfw，创建 window，设置窗口大小改变的 callback
 * @param width
 * @param height
 * @param user_data
 * @return
 */
inline GLFWwindow *init_window(int width, int height, GLFWUserData user_data)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow *window;
    window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);

    // 设置窗口大小改变的 callback
    glfwSetWindowUserPointer(window, &user_data);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int width, int height) {
        auto user_data = reinterpret_cast<GLFWUserData *>(glfwGetWindowUserPointer(window));

        user_data->framebuffer_resized = true;
    });

    return window;
}


inline vk::SurfaceKHR create_surface(const vk::Instance &instance, GLFWwindow *window)
{
    spdlog::get("logger")->info("create surface.");
    VkSurfaceKHR surface;

    // 调用 glfw 来创建 window surface，这样可以避免平台相关的细节
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create widnow surface by glfw.");

    return surface;
}


/**
 * 检查 layers 是否受支持，因为在 validation layer 启用之前没人报错，所以需要手动检查
 */
inline bool check_instance_layers(const std::vector<const char *> &layers)
{
    std::vector<vk::LayerProperties> layer_property_list = vk::enumerateInstanceLayerProperties();


    // 检查需要的 layer 是否受支持（笛卡尔积操作）
    for (const char *layer_needed: layers)
    {
        bool layer_found = false;
        for (const auto &layer_supported: layer_property_list)
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

// FIXME: dbg messenger info 通过 .pNext 传递，可能有 Bug
inline vk::Instance create_instance(const vk::DebugUtilsMessengerCreateInfoEXT &dbg_msger_create_info)
{
    spdlog::get("logger")->info("create instance.");


    // （这是可选的） 关于应用程序的信息，驱动可以根据这些信息对应用程序进行一些优化
    vk::ApplicationInfo app_info = {
            .pApplicationName   = "vk app",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_0,
    };


    // 所需的 vk 扩展
    std::vector<const char *> ext_list = {
            // validation layer 需要的扩展
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,

            // 基于 metal API 的 vulkan 实现需要这些扩展
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };
    {
        /// glfw 所需的 vk 扩展，window interface 相关的扩展
        /// VK_KHR_surface: 这个扩展可以暴露出 VkSurfaceKHR 对象，glfw 可以读取这个对象
        uint32_t glfw_ext_cnt = 0;
        const char **glfw_ext_list;
        glfw_ext_list = glfwGetRequiredInstanceExtensions(&glfw_ext_cnt);
        for (int i = 0; i < glfw_ext_cnt; ++i)
            ext_list.push_back(glfw_ext_list[i]);
    }


    // 所需的 layers
    const std::vector<const char *> layer_list = {
            "VK_LAYER_KHRONOS_validation",    // validation layer
    };
    if (!check_instance_layers(layer_list))
        throw std::runtime_error("check layers: fail.");


    // 这些数据用于指定当前应用程序需要的全局 extension 以及 validation layers
    vk::InstanceCreateInfo create_info = {
            /// 一般情况下 debug messenger 只有在 instance 创建后和销毁前才是有效的，
            /// 将这个 create info 传递给 instance 创建信息的 pNext 字段，
            /// 可以在 instance 的创建和销毁过程进行 debug
            .pNext = &dbg_msger_create_info,

            /// 表示 vulkan 除了枚举出默认的 physical device 外，
            /// 还会枚举出符合 vulkan 可移植性的 physical device
            /// 基于 metal 的 vulkan 需要这个
            .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,

            .pApplicationInfo = &app_info,

            // layers
            .enabledLayerCount   = (uint32_t) (layer_list.size()),
            .ppEnabledLayerNames = layer_list.data(),

            // extensions
            .enabledExtensionCount   = (uint32_t) ext_list.size(),
            .ppEnabledExtensionNames = ext_list.data(),
    };

    return vk::createInstance(create_info);
}


/**
 * 创建了，自动就设置到 instance 中了，返回的 handle 用于销毁
 */
inline vk::DebugUtilsMessengerEXT set_dbg_msger(const vk::Instance &instance,
                                         const vk::DebugUtilsMessengerCreateInfoEXT &dbg_msger_create_info)
{
    spdlog::get("logger")->info("set debug messenger.");
    return instance.createDebugUtilsMessengerEXT(dbg_msger_create_info);
}


inline vk::PhysicalDevice pick_physical_device(const vk::Instance &instance, const vk::SurfaceKHR &surface, GLFWwindow *window)
{
    spdlog::get("logger")->info("pick physical device.");
    // 找到所有的 physical device
    std::vector<vk::PhysicalDevice> device_list = instance.enumeratePhysicalDevices();


    /// 筛选 physical device
    /// remove_if 实际上进行的是 shift 操作，返回 iter，iter 之后都是可以移除的内容，手动移除
    auto erase_iter =
            std::remove_if(device_list.begin(), device_list.end(), [&](const vk::PhysicalDevice &physical_device) {
                DeviceInfo device_info   = DeviceInfo::get_info(physical_device, surface);
                SurfaceInfo surface_info = SurfaceInfo::get_info(physical_device, surface, window);

                if (!device_info.features.tessellationShader)
                    return false;

                if (!device_info.present_queue_family_idx.has_value() ||
                    !device_info.graphics_queue_family_idx.has_value())
                    return false;

                if (!surface_info.format_list.empty() || !surface_info.present_mode_list.empty())
                    return false;

                return true;
            });
    device_list.erase(erase_iter, device_list.end());

    if (device_list.empty())
        throw std::runtime_error("failed to find a suitable physical device.");
    return device_list[0];
}


/**
 * 创建 logical device，并获得 queue
 * @param [out]present_queue
 * @param [out]graphics_queue
 * @return
 */
inline vk::Device create_device(const vk::PhysicalDevice &physical_device, const vk::SurfaceKHR &surface,
                         vk::Queue &present_queue, vk::Queue &graphics_queue)
{
    spdlog::get("logger")->info("create device.");
    DeviceInfo device_info = DeviceInfo::get_info(physical_device, surface);


    // device 需要的 extensions
    std::vector<const char *> device_ext_list = {
            // 这是一个临时的扩展（vulkan_beta.h)，在 metal API 上模拟 vulkan 需要这个扩展
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
            // 可以将渲染结果呈现到 window surface 上
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };


    // 创建 queue 需要的信息
    std::vector<vk::DeviceQueueCreateInfo> queue_create_info_list;
    float queue_priority = 1.f;    // 每个队列的优先级，1.f 最高，0.f 最低
    // 可能同一个 queue family 支持多种特性，因此用 set 去重
    for (uint32_t queue_family: std::set<uint32_t>{device_info.graphics_queue_family_idx.value(),
                                                   device_info.present_queue_family_idx.value()})
        queue_create_info_list.push_back(vk::DeviceQueueCreateInfo{
                .queueFamilyIndex = queue_family,
                .queueCount       = 1,
                .pQueuePriorities = &queue_priority,
        });


    // locgical device 需要的 feature
    vk::PhysicalDeviceFeatures device_feature{};
    vk::DeviceCreateInfo device_create_info = {
            .queueCreateInfoCount    = (uint32_t) queue_create_info_list.size(),
            .pQueueCreateInfos       = queue_create_info_list.data(),
            .enabledExtensionCount   = (uint32_t) device_ext_list.size(),
            .ppEnabledExtensionNames = device_ext_list.data(),
            .pEnabledFeatures        = nullptr,
    };


    vk::Device device = physical_device.createDevice(device_create_info);
    present_queue     = device.getQueue(device_info.present_queue_family_idx.value(), 0);
    graphics_queue    = device.getQueue(device_info.graphics_queue_family_idx.value(), 0);
    return device;
}


inline std::pair<vk::Result, uint32_t> acquireNextImageKHR(const vk::Device &device, const vk::SwapchainKHR &swapchain,
                                                           uint64_t timeout, const vk::Semaphore &semaphore,
                                                           const vk::Fence &fence)
{
    uint32_t image_idx;
    VkResult result =
            vkAcquireNextImageKHR(static_cast<VkDevice>(device), static_cast<VkSwapchainKHR>(swapchain), timeout,
                                  static_cast<VkSemaphore>(semaphore), static_cast<VkFence>(fence), &image_idx);
    return std::make_pair(static_cast<vk::Result>(result), image_idx);
}