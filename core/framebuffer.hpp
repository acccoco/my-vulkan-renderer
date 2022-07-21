#pragma once
#include "buffer.hpp"


std::vector<vk::Framebuffer>
create_framebuffers(const Env &env, const vk::RenderPass &render_pass,
                    const std::vector<vk::ImageView> &swapchain_image_view_list,
                    const vk::ImageView &depth_img_view);


vk::Format depth_format(const Env &env);

bool stencil_component_has(const vk::Format &format);

void depth_resource_create(const Env &env, vk::Image &img, vk::DeviceMemory &mem,
                           vk::ImageView &img_view);