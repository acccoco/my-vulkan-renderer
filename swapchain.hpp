#pragma once

#include "./include_vk.hpp"
#include "./device.hpp"



inline vk::SwapchainKHR create_swapchain(const vk::Device &device, const vk::SurfaceKHR &surface,
                                         const DeviceInfo &device_info, const SurfaceInfo &surface_info)
{
    spdlog::get("logger")->info("create swapchain.");

    /// 确定 swapchain 中 image 的数量，不要超过 surface 允许的最大数量即可
    /// minImageCount 至少是 1; maxImageCount 为 0 时表示没有限制
    uint32_t image_cnt = surface_info.capability.minImageCount + 1;
    if (surface_info.capability.maxImageCount > 0 && image_cnt > surface_info.capability.maxImageCount)
        image_cnt = surface_info.capability.maxImageCount;


    // 确定 device 的 graphics queue family 和 present queue family 是否是同一个 queue family
    bool present_equal_to_graphics =
            device_info.present_queue_family_idx.value() == device_info.graphics_queue_family_idx.value();
    uint32_t queue_family_indices[] = {device_info.present_queue_family_idx.value(),
                                       device_info.graphics_queue_family_idx.value()};


    vk::SwapchainCreateInfoKHR create_info = {
            .surface          = surface,
            .minImageCount    = image_cnt,    // vulkan 可能会创建更多的 image
            .imageFormat      = surface_info.format.format,
            .imageColorSpace  = surface_info.format.colorSpace,
            .imageExtent      = surface_info.extent,    // 尺寸的单位是 pixel
            .imageArrayLayers = 1,                      // number of views, for non-stereoscopic app, it is 1
            .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = present_equal_to_graphics ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
            .queueFamilyIndexCount = uint32_t(present_equal_to_graphics ? 0 : 2),
            .pQueueFamilyIndices   = present_equal_to_graphics ? nullptr : queue_family_indices,

            // 是否需要在 present 前对 image 做一些变换，currentTransform 意味着不用任何变换
            .preTransform = surface_info.capability.currentTransform,

            // 如果一个 window 中有多个 surface，是否根据 alpha 值与其他 surface 进行复合
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,

            .presentMode  = surface_info.present_mode,
            .clipped      = VK_TRUE,    // 是否丢弃 surface 不可见区域的渲染操作，可提升性能
            .oldSwapchain = {},
    };

    return device.createSwapchainKHR(create_info);
}


inline std::vector<vk::ImageView> create_swapchain_view(const vk::Device &device, const SurfaceInfo &surface_info,
                                                        const std::vector<vk::Image> &image_list)
{
    spdlog::get("logger")->info("create swapchain view.");
    std::vector<vk::ImageView> view_list;

    for (const vk::Image &image: image_list)
    {
        vk::ImageView view = device.createImageView(vk::ImageViewCreateInfo{
                .image    = image,
                .viewType = vk::ImageViewType::e2D,
                .format   = surface_info.format.format,

                // 每个通道的映射方式：例如可以将 alpha 通道映射为一个常数
                .components = {.r = vk::ComponentSwizzle::eIdentity,
                               .g = vk::ComponentSwizzle::eIdentity,
                               .b = vk::ComponentSwizzle::eIdentity,
                               .a = vk::ComponentSwizzle::eIdentity},

                .subresourceRange = {.aspectMask     = vk::ImageAspectFlagBits::eColor,
                                     .baseMipLevel   = 0,
                                     .levelCount     = 1,
                                     .baseArrayLayer = 0,
                                     // if VR, then layer count greater than 1
                                     .layerCount = 1},
        });
        view_list.push_back(view);
    }

    return view_list;
}