#pragma once
#include "buffer.hpp"


/**
 * 创建 image 和对应的 memory，并将两者绑定
 */
void img_create(const vk::ImageCreateInfo &image_info, const vk::MemoryPropertyFlags &mem_prop,
                vk::Image &image, vk::DeviceMemory &memory);

void img_layout_trans(vk::Image &image, const vk::Format &format, const vk::ImageLayout &old_layout,
                      const vk::ImageLayout &new_layout, uint32_t mip_levels);

void buffer_image_copy(vk::Buffer &buffer, vk::Image &image, uint32_t width, uint32_t height);

vk::ImageView img_view_create(const vk::Image &tex_img, const vk::Format &format,
                              const vk::ImageAspectFlags &aspect_flags, uint32_t mip_levels);

vk::Sampler sampler_create(std::optional<uint32_t> mip_levels);

void mipmap_generate(vk::Image &image, const vk::Format &format, int32_t width, int32_t height,
                     uint32_t mip_levels);