#pragma once
#include "env.hpp"
#include "image.hpp"
#include "global.hpp"
#include "include_vk.hpp"


/**
 * 是否需要重新创建 swapchain
 */
enum class Recreate {
    NEED,
    NO_NEED,
};


class Swapchain
{
private:
    vk::SwapchainKHR _swapchain;
    std::vector<vk::Image> _images;
    std::vector<vk::ImageView> _image_views;

    Swapchain()
    {
        _swapchain   = Swapchain::create_swapchain();
        _images      = Hiss::Env::env()->device.getSwapchainImagesKHR(_swapchain);
        _image_views = Swapchain::create_swapchain_view(_images);


        LogStatic::logger()->info("[swapchain] swapchain create, image count: {}", _images.size());
    }

public:
    static std::shared_ptr<Swapchain> create()
    {
        return std::shared_ptr<Swapchain>(new Swapchain());
    }

    ~Swapchain() { this->free(); }

    vk::Format format() { return Hiss::Env::env()->present_format.format; }

    void free()
    {
        auto env = Hiss::Env::env();

        for (auto &img_view: _image_views)
            env->device.destroy(img_view);
        env->device.destroy(_swapchain);
    }


    std::vector<vk::ImageView> &img_views() { return _image_views; }


    /**
     * 从 swapchain 中获取用于绘制下一帧的 image
     * @return 检测 window 大小是否发生变化，决定是否需要 recreate swapchain
     */
    std::pair<Recreate, uint32_t> next_img_acquire(const vk::Semaphore &signal_semaphore)
    {
        auto env = Hiss::Env::env();

        /* 这里不用 vulkan-cpp，因为错误处理有些问题 */
        uint32_t image_idx;
        auto result = static_cast<vk::Result>(vkAcquireNextImageKHR(
                env->device, _swapchain, UINT64_MAX, signal_semaphore, {}, &image_idx));
        if (result == vk::Result::eErrorOutOfDateKHR)
            return std::make_pair(Recreate::NEED, 0);
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
            throw std::runtime_error("failed to acquire swapchain image.");
        return std::make_pair(Recreate::NO_NEED, image_idx);
    }


    /**
     * 告知 present queue，swapchain 上的指定 image 可以 present 了
     * @return 检测 window 大小是否发生变化，决定是否要 recreate swapchain
     */
    Recreate present(uint32_t img_idx, const std::vector<vk::Semaphore> &wait_semaphore)
    {
        vk::PresentInfoKHR info = {
                .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphore.size()),
                .pWaitSemaphores    = wait_semaphore.data(),
                .swapchainCount     = 1,
                .pSwapchains        = &_swapchain,
                .pImageIndices      = &img_idx,
        };
        vk::Result result = Hiss::Env::env()->present_queue().presentKHR(info);

        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR ||
            WindowStatic::resized())
        {
            WindowStatic::resized(false);
            return Recreate::NEED;
        }
        if (result != vk::Result::eSuccess)
            throw std::runtime_error("failed to present swapchain image.");
        return Recreate::NO_NEED;
    }


    /**
     * 回收旧的 swapchain 资源，创建新的 swapchain
     */
    void recreate() { this->free(); }

private:
    static vk::SwapchainKHR create_swapchain()
    {
        LogStatic::logger()->info("create swapchain.");
        auto env = Hiss::Env::env();


        /**
         * 确定 swapchain 中 image 的数量
         * minImageCount 至少是 1; maxImageCount 为 0 时表示没有限制
         */
        uint32_t image_cnt = env->info->surface_capability.minImageCount + 1;
        if (env->info->surface_capability.maxImageCount > 0 &&
            image_cnt > env->info->surface_capability.maxImageCount)
            image_cnt = env->info->surface_capability.maxImageCount;


        /**
        * swapchain 的 image 需要在 graphics queue 上渲染，在 present queue 上显示
        * 如果 graphics queue 和 present queue 属于不同 family，那么 image 需要共享
        */
        std::vector<uint32_t> queue_families;
        for (auto idx: std::set<uint32_t>{
                     env->graphics_queue.family_idx,
                     env->present_queue.family_idx,
             })
            queue_families.push_back(idx);


        vk::SwapchainCreateInfoKHR create_info = {
                .surface = env->surface,
                /* vulkan 可能会创建更多的 image */
                .minImageCount         = image_cnt,
                .imageFormat           = env->present_format.format,
                .imageColorSpace       = env->present_format.colorSpace,
                .imageExtent           = env->present_extent,
                .imageArrayLayers      = 1,
                .imageUsage            = vk::ImageUsageFlagBits::eColorAttachment,
                .imageSharingMode      = queue_families.size() == 1 ? vk::SharingMode::eExclusive
                                                                    : vk::SharingMode::eConcurrent,
                .queueFamilyIndexCount = static_cast<uint32_t>(queue_families.size()),
                .pQueueFamilyIndices   = queue_families.data(),
                /* 是否需要在 present 前对 image 做一些变换，currentTransform 意味着不用任何变换 */
                .preTransform = env->info->surface_capability.currentTransform,
                /* 如果一个 window 中有多个 surface，是否根据 alpha 值与其他 surface 进行复合 */
                .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                .presentMode    = env->present_mode,
                /* 是否丢弃 surface 不可见区域的渲染操作，可提升性能 */
                .clipped      = VK_TRUE,
                .oldSwapchain = {},
        };

        return env->device.createSwapchainKHR(create_info);
    }


    static std::vector<vk::ImageView>
    create_swapchain_view(const std::vector<vk::Image> &image_list)
    {
        LogStatic::logger()->info("create swapchain view.");

        std::vector<vk::ImageView> view_list;

        for (const vk::Image &image: image_list)
            view_list.push_back(img_view_create(image, Hiss::Env::env()->present_format.format,
                                                vk::ImageAspectFlagBits::eColor, 1));

        return view_list;
    }
};