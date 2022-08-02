#pragma once

#include "image.hpp"
#include "env.hpp"


class Texture
{
    vk::Image _img;
    vk::DeviceMemory _img_mem;
    vk::ImageView _img_view;
    vk::Sampler _sampler;

    uint32_t _width{};
    uint32_t _height{};
    uint32_t _channels{};
    uint32_t _mip_levels{};


    void img_init(const std::string &file_path);

public:
    static Texture load(const std::string &file_path, const vk::Format &format,
                        const vk::ImageAspectFlags &aspect)
    {
        Texture tex;
        tex.img_init(file_path);
        tex._img_view = img_view_create(tex._img, format, aspect, tex._mip_levels);
        tex._sampler  = sampler_create(tex._mip_levels);

        return tex;
    }

    vk::ImageView &img_view() { return _img_view; }
    vk::Sampler &sampler() { return _sampler; }


    void free()
    {
        auto env = Hiss::Env::env();
        env->device.destroy(_img_view);
        env->device.destroy(_img);
        env->device.destroy(_sampler);
        env->device.free(_img_mem);
    }
};