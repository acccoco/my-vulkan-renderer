#include "../framebuffer.hpp"
#include "../image.hpp"

std::vector<vk::Framebuffer>
create_framebuffers(const Env &env, const vk::RenderPass &render_pass,
                    const std::vector<vk::ImageView> &swapchain_image_view_list,
                    const vk::ImageView &depth_img_view)
{
    spdlog::get("logger")->info("create framebuffers.");

    std::vector<vk::Framebuffer> framebuffer_list;
    framebuffer_list.reserve(swapchain_image_view_list.size());
    for (const vk::ImageView &swapchain_img_view: swapchain_image_view_list)
    {
        std::vector<vk::ImageView> attachments = {
                swapchain_img_view,
                depth_img_view,
        };

        vk::FramebufferCreateInfo framebuffer_create_info = {
                .renderPass      = render_pass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments    = attachments.data(),
                .width           = env.surface_info.extent.width,
                .height          = env.surface_info.extent.height,
                .layers          = 1,
        };

        framebuffer_list.push_back(env.device.createFramebuffer(framebuffer_create_info));
    }
    return framebuffer_list;
}


/**
 * depth format 中是否包含 stencil
 */
bool stencil_component_has(const vk::Format &format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}


/**
 * 选择 device 支持的 depth format
 */
vk::Format depth_format(const Env &env)
{
    const std::vector<vk::Format> candidata_format = {
            vk::Format::eD32Sfloat,
            vk::Format::eD32SfloatS8Uint,
            vk::Format::eD24UnormS8Uint,
    };
    auto format_ = env.supported_format_find(candidata_format, vk::ImageTiling::eOptimal,
                                             vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    if (!format_.has_value())
        throw std::runtime_error("failed to find supported format.");
    return format_.value();
}


void depth_resource_create(const Env &env, vk::Image &img, vk::DeviceMemory &mem,
                           vk::ImageView &img_view)
{
    const std::vector<vk::Format> candidata_format = {
            vk::Format::eD32Sfloat,
            vk::Format::eD32SfloatS8Uint,
            vk::Format::eD24UnormS8Uint,
    };

    vk::Format depth_format;
    {
        auto format_ =
                env.supported_format_find(candidata_format, vk::ImageTiling::eOptimal,
                                          vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        if (!format_.has_value())
            throw std::runtime_error("failed to find supported format.");
        depth_format = format_.value();
    }

    vk::ImageCreateInfo img_info = {
            .imageType     = vk::ImageType::e2D,
            .format        = depth_format,
            .extent        = vk::Extent3D{.width  = (uint32_t) (env.surface_info.extent.width),
                                          .height = (uint32_t) (env.surface_info.extent.height),
                                          .depth  = 1},
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = vk::SampleCountFlagBits::e1,
            .tiling        = vk::ImageTiling::eOptimal,
            .usage         = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            .sharingMode   = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
    };

    img_create(env, img_info, vk::MemoryPropertyFlagBits::eDeviceLocal, img, mem);
    img_view = img_view_create(env, img, depth_format, vk::ImageAspectFlagBits::eDepth, 1);

    // 这一步是可选的，在 render pass 中会做
    img_layout_trans(env, img, depth_format, vk::ImageLayout::eUndefined,
                     vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);
}