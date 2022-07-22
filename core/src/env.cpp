#include "../env.hpp"
#include "../buffer.hpp"
#include "../image.hpp"

PhysicalInfo::PhysicalInfo(const vk::PhysicalDevice &physical_device, const vk::SurfaceKHR &surface)
{
    physical_device_properties = physical_device.getProperties();
    physical_device_features   = physical_device.getFeatures();
    pdevice_mem_props          = physical_device.getMemoryProperties();
    queue_family_properties    = physical_device.getQueueFamilyProperties();
    support_ext                = physical_device.enumerateDeviceExtensionProperties();

    surface_capability  = physical_device.getSurfaceCapabilitiesKHR(surface);
    surface_format_list = physical_device.getSurfaceFormatsKHR(surface);
    present_mode_list   = physical_device.getSurfacePresentModesKHR(surface);

    /* 找到合适的 queue family */
    for (uint32_t i = 0; i < queue_family_properties.size(); ++i)
    {
        if (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics)
            grahics_queue_families.push_back(i);
        if (physical_device.getSurfaceSupportKHR(i, surface))
            present_queue_families.push_back(i);
        if (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eTransfer)
            transfer_queue_families.push_back(i);
    }
}


vk::SurfaceKHR EnvSingleton::surface_create(const vk::Instance &instance, GLFWwindow *window)
{
    /* 调用 glfw 来创建 window surface，这样可以避免平台相关的细节 */
    VkSurfaceKHR surface_;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface_) != VK_SUCCESS)
        throw std::runtime_error("failed to create widnow surface by glfw.");
    return surface_;
}


bool EnvSingleton::physical_device_pick(const PhysicalInfo &info)
{
    /* device feature：需要支持 tessellation 以及 anisotropy sample */
    if (!info.physical_device_features.tessellationShader ||
        !info.physical_device_features.samplerAnisotropy)
        return false;

    /* 有合适的 queue family */
    if (info.present_queue_families.empty() || info.grahics_queue_families.empty() ||
        info.transfer_queue_families.empty())
        return false;

    /* 有合适的 format 以及 present mode */
    if (info.surface_format_list.empty() || info.present_queue_families.empty())
        return false;

    return true;
}


/**
 * 创建 logical device，申请需要的 queue
 */
vk::Device EnvSingleton::device_create(const vk::PhysicalDevice &physical_device,
                                       const PhysicalInfo &physical_info,
                                       const std::vector<vk::DeviceQueueCreateInfo> &queue_info)
{
    /* device 需要的 extensions */
    std::vector<const char *> device_ext_list = {
            /* 这是一个临时的扩展（vulkan_beta.h)，在 metal API 上模拟 vulkan 需要这个扩展 */
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
            /* 可以将渲染结果呈现到 window surface 上 */
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    /* locgical device 需要的 feature */
    [[maybe_unused]] vk::PhysicalDeviceFeatures device_feature{
            .tessellationShader = VK_TRUE,
            .sampleRateShading  = VK_TRUE,
            .samplerAnisotropy  = VK_TRUE,
    };
    vk::DeviceCreateInfo device_create_info = {
            .queueCreateInfoCount    = (uint32_t) queue_info.size(),
            .pQueueCreateInfos       = queue_info.data(),
            .enabledExtensionCount   = (uint32_t) device_ext_list.size(),
            .ppEnabledExtensionNames = device_ext_list.data(),
            .pEnabledFeatures        = &device_feature,
    };

    return physical_device.createDevice(device_create_info);
}

vk::CommandPool EnvSingleton::cmd_pool_create(const vk::Device &device, uint32_t queue_family_indx)
{
    vk::CommandPoolCreateInfo pool_create_info = {
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queue_family_indx,
    };
    return device.createCommandPool(pool_create_info);
}

/**
 * 从设备支持的 format 中选择一种，优先选择 BGRA8_srgb 的
 */
vk::SurfaceFormatKHR
EnvSingleton::present_format_choose(const std::vector<vk::SurfaceFormatKHR> &format_list_)
{
    for (const auto &format: format_list_)
        if (format.format == vk::Format::eB8G8R8A8Srgb &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return format;
    return format_list_[0];
}

/**
 * 从设备支持的 present mode 中选择一种，优先选择 mailbox 类型
 */
vk::PresentModeKHR
EnvSingleton::present_mode_choose(const std::vector<vk::PresentModeKHR> &present_mode_list_)
{
    for (const auto &present_mode: present_mode_list_)
        if (present_mode == vk::PresentModeKHR::eMailbox)
            return present_mode;
    return vk::PresentModeKHR::eFifo;
}


/**
 * 根据 glfw window 的大小，获得当前的 extent(pixel)
 * vulkan 使用的是以 pixel 为单位的分辨率，glfw 初始化窗口时使用的是 screen coordinate
 * 为单位的分辨率 在 Apple Retina display 上，pixel 是 screen coordinate 的 2 倍 最好的方法就是使用
 * glfwGetFramebufferSize 去查询 window 以 pixel 为单位的大小
 */
vk::Extent2D EnvSingleton::present_extent_choose(vk::SurfaceCapabilitiesKHR &capability_,
                                                 GLFWwindow *window)
{
    /* 如果是 0xFFFFFFFF，表示 extent 需要由相关的 swapchain 大小来决定，也就是手动决定 */
    if (capability_.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capability_.currentExtent;

    /* 询问 glfw，当前窗口的大小是多少（pixel） */
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return vk::Extent2D{
            .width  = std::clamp((uint32_t) width, capability_.minImageExtent.width,
                                 capability_.maxImageExtent.width),
            .height = std::clamp((uint32_t) height, capability_.minImageExtent.height,
                                 capability_.maxImageExtent.height),
    };
}


/**
 * 在 instance 以及 window 之后初始化，在 env 之前销毁
 */
void EnvSingleton::init_once(const vk::Instance &instance)
{
    assert(_env == nullptr);

    auto logger = LogStatic::logger();
    EnvSingleton env;

    /* 创建 surface 以及 physical device */
    logger->info("create surface and physical device.");
    env.surface                = EnvSingleton::surface_create(instance, WindowStatic::window_get());
    bool physical_device_found = false;
    for (const auto &physical_device: instance.enumeratePhysicalDevices())
    {
        env.info = std::make_shared<PhysicalInfo>(physical_device, env.surface);
        if (physical_device_pick(*env.info))
        {
            env.physical_device   = physical_device;
            physical_device_found = true;
            break;
        }
    }
    if (!physical_device_found)
        throw std::runtime_error("failed to find a suitable physical device.");

    /**
     * 创建 logical device 以及 queue，command pool
     * 同一个 queue family 可能支持多种功能，因此用 set 去重
     */
    logger->info("create device and queue.");
    std::vector<vk::DeviceQueueCreateInfo> queue_info;
    float queue_priority = 1.f; /* 每个队列的优先级，1.f 最高，0.f 最低 */
    for (uint32_t queue_family_idx: std::set<uint32_t>{env.info->grahics_queue_families[0],
                                                       env.info->present_queue_families[0],
                                                       env.info->transfer_queue_families[0]})
    {
        queue_info.push_back({
                .queueFamilyIndex = queue_family_idx,
                .queueCount       = 1,
                .pQueuePriorities = &queue_priority,
        });
    }
    env.device         = device_create(env.physical_device, *env.info, queue_info);
    env.graphics_queue = {
            .queue      = env.device.getQueue(env.info->grahics_queue_families[0], 0),
            .family_idx = env.info->grahics_queue_families[0],
    };
    env.present_queue = {
            .queue      = env.device.getQueue(env.info->present_queue_families[0], 0),
            .family_idx = env.info->present_queue_families[0],
    };
    env.transfer_queue = {
            .queue      = env.device.getQueue(env.info->present_queue_families[0], 0),
            .family_idx = env.info->present_queue_families[0],
    };
    env.graphics_cmd_pool = {
            .pool         = cmd_pool_create(env.device, env.info->grahics_queue_families[0]),
            .commit_queue = env.graphics_queue,
    };

    /* 确定 surface 相关的属性 */
    env.present_format = present_format_choose(env.info->surface_format_list);
    env.present_mode   = present_mode_choose(env.info->present_mode_list);
    env.present_extent =
            present_extent_choose(env.info->surface_capability, WindowStatic::window_get());

    _env = std::make_shared<EnvSingleton>(env);
}


void EnvSingleton::free(const vk::Instance &instance)
{
    assert(_env != nullptr);

    _env->device.destroy(_env->graphics_cmd_pool.pool);
    _env->device.destroy();
    instance.destroy(_env->surface);

    _env = nullptr; /* 对 shared_ptr 赋值 nullptr，会自动调用原来的析构函数 */
}


/**
 * window 尺寸发生变换，因此重新创建 surface，physical info 也要一起更新
 */
void EnvSingleton::surface_recreate(const vk::Instance &instance)
{
    assert(_env != nullptr);

    instance.destroy(_env->surface);
    _env->surface = EnvSingleton::surface_create(instance, WindowStatic::window_get());
    _env->info    = std::make_shared<PhysicalInfo>(_env->physical_device, _env->surface);
    _env->present_extent =
            present_extent_choose(_env->info->surface_capability, WindowStatic::window_get());
}


/**
 * 根据 tiling 和 features，在 candidate 中找到合适的 format
 */
std::optional<vk::Format> EnvSingleton::format_filter(const std::vector<vk::Format> &candidates,
                                                      vk::ImageTiling tiling,
                                                      vk::FormatFeatureFlags features)
{
    assert(_env != nullptr);

    for (const vk::Format &format: candidates)
    {
        vk::FormatProperties props = _env->physical_device.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear &&
            BITS_CONTAIN(props.linearTilingFeatures, features))
            return format;
        if (tiling == vk::ImageTiling::eOptimal &&
            BITS_CONTAIN(props.optimalTilingFeatures, features))
            return format;
    }
    return std::nullopt;
}


/**
 * 找到 device 允许的最大 MSAA 数
 */
vk::SampleCountFlagBits EnvSingleton::max_sample_cnt()
{
    assert(_env != nullptr);

    /* 在 color 和 depth 都支持的 sample count 中，取最大值 */
    vk::SampleCountFlags counts =
            _env->info->physical_device_properties.limits.framebufferColorSampleCounts &
            _env->info->physical_device_properties.limits.framebufferDepthSampleCounts;
    for (auto bit: {
                 vk::SampleCountFlagBits::e64,
                 vk::SampleCountFlagBits::e32,
                 vk::SampleCountFlagBits::e16,
                 vk::SampleCountFlagBits::e8,
                 vk::SampleCountFlagBits::e4,
                 vk::SampleCountFlagBits::e2,
         })
        if (counts & bit)
            return bit;
    return vk::SampleCountFlagBits::e1;
}


vk::DeviceMemory EnvSingleton::mem_allocate(const vk::MemoryRequirements &mem_require,
                                            const vk::MemoryPropertyFlags &mem_prop)
{
    assert(_env != nullptr);

    /* 根据 mem require 和 properties 在 device 中找到合适的 memory type，获得其 index */
    std::optional<uint32_t> mem_idx;
    for (uint32_t i = 0; i < _env->info->pdevice_mem_props.memoryTypeCount; ++i)
    {
        if (!(mem_require.memoryTypeBits & (1 << i)))
            continue;
        if (!BITS_CONTAIN(_env->info->pdevice_mem_props.memoryTypes[i].propertyFlags, mem_prop))
            continue;
        mem_idx = i;
    }
    if (!mem_idx.has_value())
        throw std::runtime_error("no proper memory type for buffer, didn't allocate buffer.");

    return _env->device.allocateMemory({
            .allocationSize  = mem_require.size,
            .memoryTypeIndex = mem_idx.value(),
    });
}