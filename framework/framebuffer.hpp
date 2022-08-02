#pragma once
#include "attachment.hpp"
#include "env.hpp"


struct FramebufferLayout_temp {
    vk::Format color_format;
    vk::SampleCountFlagBits color_sample;

    vk::Format depth_format;
    vk::SampleCountFlagBits depth_sample;

    vk::Format resolve_format;
    vk::SampleCountFlagBits resolve_sample;
};


/**
 * 共 3 个 attachment：color，depth，resolve
 * MSAA 使用最大值
 */
class MSAAFramebuffer
{
private:
    std::vector<vk::Framebuffer> _framebuffers;
    std::shared_ptr<DepthAttachment> _depth_attach;
    std::shared_ptr<ColorAttachment> _color_attach;

    MSAAFramebuffer(const vk::RenderPass &render_pass,
                    const FramebufferLayout_temp &framebuffer_layout,
                    const std::vector<vk::ImageView> &resolve_views, const vk::Extent2D &extent)
    {
        auto env = Hiss::Env::env();


        /* 创建各种 attachment */
        _color_attach = ColorAttachment::create(framebuffer_layout.color_format, extent,
                                                framebuffer_layout.color_sample);
        _depth_attach = DepthAttachment::create(extent, framebuffer_layout.depth_sample,
                                                framebuffer_layout.depth_format);
        std::array<vk::ImageView, 3> attachments = {
                _color_attach->image_view(), _depth_attach->image_view(),
                /* resolve attachment 在后面动态填充 */
        };


        vk::FramebufferCreateInfo framebuffer_create_info = {
                .renderPass      = render_pass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments    = attachments.data(),
                .width           = extent.width,
                .height          = extent.height,
                .layers          = 1,
        };
        _framebuffers.reserve(resolve_views.size());
        for (const auto &resolve_view: resolve_views)
        {
            attachments[2] = resolve_view;
            _framebuffers.push_back(Hiss::Env::env()->device.createFramebuffer(
                    framebuffer_create_info));
        }
    }

public:
    static std::shared_ptr<MSAAFramebuffer> create(const vk::RenderPass &render_pass,
                                                   const FramebufferLayout_temp &framebuffer_layout,
                                                   const std::vector<vk::ImageView> &resolve_views,
                                                   const vk::Extent2D &extent)
    {
        return std::shared_ptr<MSAAFramebuffer>(
                new MSAAFramebuffer(render_pass, framebuffer_layout, resolve_views, extent));
    }


    vk::Framebuffer &framebuffer_get(uint32_t swapchain_img_idx)
    {
        return _framebuffers[swapchain_img_idx];
    }


    void free()
    {
        auto env = Hiss::Env::env();

        for (auto &framebuffer: _framebuffers)
            env->device.destroy(framebuffer);
        _depth_attach->free();
        _color_attach->free();
    }

    ~MSAAFramebuffer() { this->free(); }
};
