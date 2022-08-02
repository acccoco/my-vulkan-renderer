#include "../device.hpp"
#include "window.hpp"
#include <set>


bool Hiss::Device::physical_device_pick(vk::Instance instance, vk::SurfaceKHR surface)
{
    for (auto physical_device: instance.enumeratePhysicalDevices())
    {
        /* 筛选 feature */
        auto features = physical_device.getFeatures();
        if (!features.tessellationShader || !features.samplerAnisotropy)
            continue;


        /* 找到 queue family index */
        std::vector<uint32_t> graphics, present, compute;
        auto                  queue_properties = physical_device.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queue_properties.size(); ++i)
        {
            if (queue_properties[i].queueFlags & vk::QueueFlagBits::eGraphics)
                graphics.push_back(i);
            if (queue_properties[i].queueFlags & vk::QueueFlagBits::eCompute)
                compute.push_back(i);
            if (physical_device.getSurfaceSupportKHR(i, surface))
                present.push_back(i);
        }
        if (graphics.empty() || compute.empty() || present.empty())
            return false;
        _present_queue_family_index  = present;
        _graphics_queue_family_index = graphics;
        _compute_queue_family_index  = compute;


        _physical_device = physical_device;
        return true;
    }

    return false;
}


Hiss::Device::Device(vk::Instance instance, vk::SurfaceKHR surface, const Hiss::Window &window)
{
    if (physical_device_pick(instance, surface))
        throw std::runtime_error("cannot find suitable physical device.");

    logical_device_create();
    present_format_choose(surface);
    present_mode_choose(surface);
    surface_extent_choose(surface, window);
}


void Hiss::Device::logical_device_create()
{
    /* queue 的创建信息 */
    std::vector<vk::DeviceQueueCreateInfo> queue_info;
    float                                  queue_priority = 1.f;
    for (uint32_t queue_family_idx: std::set<uint32_t>{_graphics_queue_family_index[0], _present_queue_family_index[0],
                                                       _compute_queue_family_index[0]})
    {
        queue_info.push_back(vk::DeviceQueueCreateInfo{.queueFamilyIndex = queue_family_idx,
                                                       .queueCount       = 1,
                                                       .pQueuePriorities = &queue_priority});
    }


    /* extensions */
    std::vector<const char *> device_ext_list = {
            /* 这是一个临时的扩展（vulkan_beta.h)，在 metal API 上模拟 vulkan 需要这个扩展 */
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
            /* 可以将渲染结果呈现到 window surface 上 */
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };


    /* feature */
    [[maybe_unused]] vk::PhysicalDeviceFeatures device_feature{
            .tessellationShader = VK_TRUE,
            .sampleRateShading  = VK_TRUE,
            .samplerAnisotropy  = VK_TRUE,
    };


    _device = _physical_device.createDevice(vk::DeviceCreateInfo{
            .queueCreateInfoCount    = (uint32_t) queue_info.size(),
            .pQueueCreateInfos       = queue_info.data(),
            .enabledExtensionCount   = (uint32_t) device_ext_list.size(),
            .ppEnabledExtensionNames = device_ext_list.data(),
            .pEnabledFeatures        = &device_feature,
    });


    /* 获取 queue */
    _graphics_queue = {.queue        = _device.getQueue(_graphics_queue_family_index[0], 0),
                       .family_index = _graphics_queue_family_index[0]};
    _present_queue  = {.queue        = _device.getQueue(_present_queue_family_index[0], 0),
                       .family_index = _present_queue_family_index[0]};
    _compute_queue  = {.queue        = _device.getQueue(_compute_queue_family_index[0], 0),
                       .family_index = _compute_queue_family_index[0]};
}


void Hiss::Device::present_format_choose(vk::SurfaceKHR surface)
{
    std::vector<vk::SurfaceFormatKHR> format_list = _physical_device.getSurfaceFormatsKHR(surface);
    for (const auto &format: format_list)
        if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            _present_format = format;
    _present_format = format_list[0];
}


void Hiss::Device::present_mode_choose(vk::SurfaceKHR surface)
{
    std::vector<vk::PresentModeKHR> present_mode_list = _physical_device.getSurfacePresentModesKHR(surface);
    for (const auto &present_mode: present_mode_list)
        if (present_mode == vk::PresentModeKHR::eMailbox)
            _present_mode = present_mode;
    _present_mode = vk::PresentModeKHR::eFifo;
}


void Hiss::Device::surface_extent_choose(vk::SurfaceKHR surface, const Hiss::Window &window)
{
    auto capability = _physical_device.getSurfaceCapabilitiesKHR(surface);


    /* 如果是 0xFFFFFFFF，表示 extent 需要由相关的 swapchain 大小来决定，也就是手动决定 */
    if (capability.currentExtent.width != std::numeric_limits<uint32_t>::max())
        _present_extent = capability.currentExtent;

    /* 询问 glfw，当前窗口的大小是多少（pixel） */
    auto window_extent = window.extent_get();

    _present_extent = vk::Extent2D{
            .width  = std::clamp(window_extent.width, capability.minImageExtent.width, capability.maxImageExtent.width),
            .height = std::clamp(window_extent.height, capability.minImageExtent.height,
                                 capability.maxImageExtent.height),
    };
}
