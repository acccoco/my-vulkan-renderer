#pragma once

#include <set>
#include <vector>
#include <optional>

#include "./include_vk.hpp"


struct DeviceInfo {
    vk::PhysicalDeviceProperties physical_device_properties;
    vk::PhysicalDeviceFeatures physical_device_features;
    vk::PhysicalDeviceMemoryProperties physical_device_memory_properties;

    std::vector<vk::QueueFamilyProperties> queue_family_property_list;
    std::optional<uint32_t> graphics_queue_family_idx;
    std::optional<uint32_t> present_queue_family_idx;

    std::vector<vk::ExtensionProperties> support_ext_list;


    static DeviceInfo get_info(const vk::PhysicalDevice &physical_device,
                               const vk::SurfaceKHR &surface)
    {
        DeviceInfo info{
                .physical_device_properties        = physical_device.getProperties(),
                .physical_device_features          = physical_device.getFeatures(),
                .physical_device_memory_properties = physical_device.getMemoryProperties(),
                .queue_family_property_list        = physical_device.getQueueFamilyProperties(),
                .support_ext_list = physical_device.enumerateDeviceExtensionProperties(),
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


    /**
     * 根据 memory require，找到合适的 memory type，返回其在 device 中的 index
     */
    std::optional<uint32_t> find_memory_type(vk::MemoryRequirements &mem_require,
                                             const vk::MemoryPropertyFlags &mem_property) const
    {
        // 找到合适的 memory type，获得其 index
        std::optional<uint32_t> mem_type_idx;

        for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; ++i)
        {
            if (!(mem_require.memoryTypeBits & (1 << i)))
                continue;
            if ((vk::MemoryPropertyFlags(
                         physical_device_memory_properties.memoryTypes[i].propertyFlags) &
                 mem_property) != mem_property)
                continue;
            mem_type_idx = i;
            break;
        }

        return mem_type_idx;
    }
};


struct SurfaceInfo {
    vk::SurfaceCapabilitiesKHR capability;
    std::vector<vk::SurfaceFormatKHR> format_list;
    std::vector<vk::PresentModeKHR> present_mode_list;

    vk::SurfaceFormatKHR format;
    vk::PresentModeKHR present_mode;
    vk::Extent2D extent;


    static SurfaceInfo get_info(const vk::PhysicalDevice &physical_device,
                                const vk::SurfaceKHR &surface, GLFWwindow *window)
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
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return format;
        return format_list_[0];
    }


    static vk::PresentModeKHR
    choose_present_mode(const std::vector<vk::PresentModeKHR> &present_mode_list_)
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
        extent.width  = std::clamp((uint32_t) width, capability_.minImageExtent.width,
                                   capability_.maxImageExtent.width);
        extent.height = std::clamp((uint32_t) height, capability_.minImageExtent.height,
                                   capability_.maxImageExtent.height);
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
GLFWwindow *init_window(int width, int height, GLFWUserData user_data);


vk::SurfaceKHR create_surface(const vk::Instance &instance, GLFWwindow *window);


/**
 * 检查 layers 是否受支持，因为在 validation layer 启用之前没人报错，所以需要手动检查
 */
bool check_instance_layers(const std::vector<const char *> &layers);


vk::Instance create_instance(const vk::DebugUtilsMessengerCreateInfoEXT &dbg_msger_create_info);


/**
 * 创建了，自动就设置到 instance 中了，返回的 handle 用于销毁
 */
vk::DebugUtilsMessengerEXT
set_dbg_msger(const vk::Instance &instance,
              const vk::DebugUtilsMessengerCreateInfoEXT &dbg_msger_create_info);


vk::PhysicalDevice pick_physical_device(const vk::Instance &instance, const vk::SurfaceKHR &surface,
                                        GLFWwindow *window);


/**
 * 创建 logical device，并获得 queue
 * @param [out]present_queue
 * @param [out]graphics_queue
 * @return
 */
vk::Device create_device(const vk::PhysicalDevice &physical_device, const vk::SurfaceKHR &surface,
                         vk::Queue &present_queue, vk::Queue &graphics_queue);


std::pair<vk::Result, uint32_t>
acquireNextImageKHR(const vk::Device &device, const vk::SwapchainKHR &swapchain, uint64_t timeout,
                    const vk::Semaphore &semaphore, const vk::Fence &fence);


vk::SwapchainKHR create_swapchain(const vk::Device &device, const vk::SurfaceKHR &surface,
                                  const DeviceInfo &device_info, const SurfaceInfo &surface_info);


std::vector<vk::ImageView> create_swapchain_view(const vk::Device &device,
                                                 const SurfaceInfo &surface_info,
                                                 const std::vector<vk::Image> &image_list);