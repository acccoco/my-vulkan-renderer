#include "../texture.hpp"


void Texture::img_init(const std::string &file_path)
{
    auto env = EnvSingleton::env();

    /* read data from texture file */
    stbi_uc *data = nullptr;
    {
        int width, height, channels;
        data = stbi_load(file_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!data)
            throw std::runtime_error("failed to load texture.");
        _width      = width;
        _height     = height;
        _channels   = channels;
        _mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    }
    vk::DeviceSize image_size = _width * _height * 4;


    /* texture data -> stage buffer */
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_memory;
    buffer_create(image_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  stage_buffer, stage_memory);
    void *stage_data = env->device.mapMemory(stage_memory, 0, image_size, {});
    std::memcpy(stage_data, data, static_cast<size_t>(image_size));
    env->device.unmapMemory(stage_memory);


    /* create an image and memory */
    vk::ImageCreateInfo image_info = {
            .imageType   = vk::ImageType::e2D,
            .format      = vk::Format::eR8G8B8A8Srgb,    // 和图片文件保持一致
            .extent      = vk::Extent3D{.width = _width, .height = _height, .depth = 1},
            .mipLevels   = _mip_levels,
            .arrayLayers = 1,
            .samples     = vk::SampleCountFlagBits::e1,
            .tiling      = vk::ImageTiling::eOptimal,
            .usage       = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferSrc,
            .sharingMode   = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
    };
    img_create(image_info, vk::MemoryPropertyFlagBits::eDeviceLocal, _img, _img_mem);


    /* stage buffer -> image */
    img_layout_trans(_img, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined,
                     vk::ImageLayout::eTransferDstOptimal, _mip_levels);
    // 向 mipmap 的 level 0 写入
    buffer_image_copy(stage_buffer, _img, _width, _height);
    // 基于 level 0 创建其他的 level
    mipmap_generate(_img, vk::Format::eR8G8B8A8Srgb, static_cast<int32_t>(_width),
                    static_cast<int32_t>(_height), _mip_levels);


    // free resource
    stbi_image_free(data);
    env->device.destroyBuffer(stage_buffer);
    env->device.freeMemory(stage_memory);
}