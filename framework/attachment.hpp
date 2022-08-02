#pragma once
#include "image.hpp"
#include "env.hpp"


class AttachmentBase
{
protected:
    vk::Image _img;
    vk::DeviceMemory _mem;
    vk::ImageView _view;

    vk::Format _format{};


public:
    void free()
    {
        auto env = Hiss::Env::env();
        env->device.destroy(_view);
        env->device.destroy(_img);
        env->device.free(_mem);
    }

    vk::ImageView &image_view() { return _view; }
};


class DepthAttachment : public AttachmentBase
{
private:
    DepthAttachment() = default;

public:
    static std::shared_ptr<DepthAttachment>
    create(const vk::Extent2D &extent, vk::SampleCountFlagBits msaa, const vk::Format &format)
    {
        auto env = Hiss::Env::env();

        /* 找到合适的 depth format */
        auto attach     = std::shared_ptr<DepthAttachment>(new DepthAttachment());
        attach->_format = format;

        vk::ImageCreateInfo img_info = {
                .imageType = vk::ImageType::e2D,
                .format    = attach->_format,
                .extent = vk::Extent3D{.width = extent.width, .height = extent.height, .depth = 1},
                /* render target，不需要 mipmap */
                .mipLevels   = 1,
                .arrayLayers = 1,
                /* 使用最高级的 MSAA */
                .samples       = msaa,
                .tiling        = vk::ImageTiling::eOptimal,
                .usage         = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                .sharingMode   = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined,
        };

        img_create(img_info, vk::MemoryPropertyFlagBits::eDeviceLocal, attach->_img, attach->_mem);
        attach->_view =
                img_view_create(attach->_img, attach->_format, vk::ImageAspectFlagBits::eDepth, 1);

        return attach;
    }
};


class ColorAttachment : public AttachmentBase
{
public:
    static std::shared_ptr<ColorAttachment>
    create(const vk::Format &format, const vk::Extent2D &extent, vk::SampleCountFlagBits msaa)
    {
        auto attach     = std::shared_ptr<ColorAttachment>(new ColorAttachment());
        auto env        = Hiss::Env::env();
        attach->_format = format;


        vk::ImageCreateInfo img_info = {
                .imageType   = vk::ImageType::e2D,
                .format      = attach->_format,
                .extent      = {.width = extent.width, .height = extent.height, .depth = 1},
                .mipLevels   = 1,
                .arrayLayers = 1,
                .samples     = msaa,
                .tiling      = vk::ImageTiling::eOptimal,

                .usage = vk::ImageUsageFlagBits::eTransientAttachment
                         | vk::ImageUsageFlagBits::eColorAttachment,
                .sharingMode   = vk::SharingMode::eExclusive,
                .initialLayout = vk::ImageLayout::eUndefined,
        };
        img_create(img_info, vk::MemoryPropertyFlagBits::eDeviceLocal, attach->_img, attach->_mem);
        attach->_view =
                img_view_create(attach->_img, attach->_format, vk::ImageAspectFlagBits::eColor, 1);

        return attach;
    }

private:
    ColorAttachment() = default;
};