#include "../vk_common.hpp"
#include "../image.hpp"
#include "env.hpp"


void img_create(const vk::ImageCreateInfo &image_info, const vk::MemoryPropertyFlags &mem_prop, vk::Image &image,
                vk::DeviceMemory &memory)
{
    auto env = *Hiss::Env::env();
    image    = env.device.createImage(image_info);

    vk::MemoryRequirements mem_require = env.device.getImageMemoryRequirements(image);

    memory = Hiss::Env::mem_allocate(mem_require, mem_prop);

    env.device.bindImageMemory(image, memory, 0);
}


/**
 * 使用 image memory barrier 来转换 image layout
 */
void img_layout_trans(vk::Image &image, const vk::Format &format, const vk::ImageLayout &old_layout,
                      const vk::ImageLayout &new_layout, uint32_t mip_levels)
{
    vk::ImageAspectFlags aspect_flags = vk::ImageAspectFlagBits::eColor;
    if (new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        aspect_flags = vk::ImageAspectFlagBits::eDepth;
        if (Hiss::stencil_component_has(format))
            aspect_flags |= vk::ImageAspectFlagBits::eStencil;
    }


    // barrier 有三个作用：synchronize, layout 转换, queue family ownership
    vk::ImageMemoryBarrier barrier = {
            // 这两个参数用于转换 image layout
            .oldLayout = old_layout,
            .newLayout = new_layout,

            /**
             * 这两个参数用于转换 queue family ownership
             * 使用 share mode excessive 可以不用考虑 ownership 转换，但是性能较低
             */
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

            .image = image,

            // 图像的哪些区域会受影响
            .subresourceRange =
                    vk::ImageSubresourceRange{
                            .aspectMask     = aspect_flags,
                            .baseMipLevel   = 0,
                            .levelCount     = mip_levels,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
                    },
    };


    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;

    if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal)
    {
        // 注：transfer 是一个 pseudo 的 pipeline stage，用于进行 transfer 的
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal
               && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (old_layout == vk::ImageLayout::eUndefined
               && new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask =
                vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;

        /**
         * depth buffer 在 early fragment test 阶段被 read，在 late fragment test 阶段发生 write
         * 取最早的 stage
         * FIXME 按我的理解，layout transition 应该是马上发生(one time cmd)，这里指定 dst stage 是否有必要呢？
         */
        dst_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    }

    else
        throw std::invalid_argument("unsupported layout transition!");


    auto             env = Hiss::Env::env();
    OneTimeCmdBuffer cmd;
    // 前三个参数：src 和 dst 的 pipeline stage；xxx
    // 后三个参数：三种 barrier 的 list
    cmd().pipelineBarrier(src_stage, dst_stage, {}, {}, {}, {barrier});
    cmd.end();
}


void buffer_image_copy(vk::Buffer &buffer, vk::Image &image, uint32_t width, uint32_t height)
{
    vk::BufferImageCopy copy_region = {
            // buffer 中 pixel 的起始位置
            .bufferOffset = 0,

            // pixel 是如何布局的。两者是 0 表示 tightly packed
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,

            // image 的哪些区域需要 copy
            .imageSubresource =
                    vk::ImageSubresourceLayers{
                            .aspectMask     = vk::ImageAspectFlagBits::eColor,
                            .mipLevel       = 0,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
                    },
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, 1},
    };


    auto             env = *Hiss::Env::env();
    OneTimeCmdBuffer cmd;
    // 第 3 个参数：image 当前是什么 layout，这里表示适合用于 transfer to
    cmd().copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {copy_region});

    cmd.end();
}


vk::ImageView img_view_create(const vk::Image &tex_img, const vk::Format &format,
                              const vk::ImageAspectFlags &aspect_flags, uint32_t mip_levels)
{
    vk::ImageViewCreateInfo view_info = {
            .image            = tex_img,
            .viewType         = vk::ImageViewType::e2D,
            .format           = format,
            .subresourceRange = {.aspectMask     = aspect_flags,
                                 .baseMipLevel   = 0,
                                 .levelCount     = mip_levels,
                                 .baseArrayLayer = 0,
                                 .layerCount     = 1},
    };

    return Hiss::Env::env()->device.createImageView(view_info);
}


vk::Sampler sampler_create(std::optional<uint32_t> mip_levels)
{
    auto                  env          = Hiss::Env::env();
    vk::SamplerCreateInfo sampler_info = {
            .magFilter  = vk::Filter::eLinear,
            .minFilter  = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,

            .addressModeU = vk::SamplerAddressMode::eMirroredRepeat,
            .addressModeV = vk::SamplerAddressMode::eMirroredRepeat,
            .addressModeW = vk::SamplerAddressMode::eMirroredRepeat,

            .mipLodBias = 0.f,

            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy    = env->info->physical_device_properties.limits.maxSamplerAnisotropy,

            // 在 PCF shadow map 中会用到
            .compareEnable = VK_FALSE,
            .compareOp     = vk::CompareOp::eAlways,

            .minLod = 0.f,
            .maxLod = static_cast<float>(mip_levels.has_value() ? mip_levels.value() : 0),

            .borderColor             = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = VK_FALSE,
    };
    return env->device.createSampler(sampler_info);
}


/**
 * 通过 blit 一层一层地生成 mipmap
 * 确保 image 是支持 mipmap 的（也就是有足够的空间）
 * 假定 image 原来的 layout 是 transfer dst；函数执行后会将 layout 转换为 shader read only
 */
void mipmap_generate(vk::Image &image, const vk::Format &format, int32_t width, int32_t height, uint32_t mip_levels)
{
    auto env = Hiss::Env::env();

    /* 检查 platform 上的 format 是否支持 linear filter */
    {
        vk::FormatProperties format_prop = env->physical_device.getFormatProperties(format);
        if (!(format_prop.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
            throw std::runtime_error("texture image format does not support linear filtering.");
    }


    OneTimeCmdBuffer       cmd_buffer;
    vk::ImageMemoryBarrier barrier = {
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = image,
            .subresourceRange    = {.aspectMask     = vk::ImageAspectFlagBits::eColor,
                                    .levelCount     = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1},
    };


    int32_t mip_width  = width;
    int32_t mip_height = height;
    for (uint32_t level = 1; level < mip_levels; ++level)
    {
        /* 根据 level-1 生成 level，确保 level-1 在被读取之前是 transfer src 的 layout */
        barrier.subresourceRange.baseMipLevel = level - 1;
        barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout                     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask                 = vk::AccessFlagBits::eTransferRead;
        cmd_buffer().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {},
                                     {}, {barrier});


        /* 根据 level-1 生成 level，使用 linear 的方法来生成 */
        auto src_offset = std::array<vk::Offset3D, 2>{
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{mip_width, mip_height, 1},
        };
        auto dst_offset = std::array<vk::Offset3D, 2>{
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{std::max(1, mip_width / 2), std::max(1, mip_height / 2), 1},
        };
        vk::ImageBlit blit = {
                .srcSubresource = {.aspectMask     = vk::ImageAspectFlagBits::eColor,
                                   .mipLevel       = level - 1,
                                   .baseArrayLayer = 0,
                                   .layerCount     = 1},
                .srcOffsets     = src_offset,
                .dstSubresource = {.aspectMask     = vk::ImageAspectFlagBits::eColor,
                                   .mipLevel       = level,
                                   .baseArrayLayer = 0,
                                   .layerCount     = 1},
                .dstOffsets     = dst_offset,
        };
        cmd_buffer().blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal,
                               {blit}, vk::Filter::eLinear);


        /* 将 level-1 image 变为最终的 shader read only layout */
        barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        cmd_buffer().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                                     {}, {}, {}, {barrier});


        if (mip_width > 1)
            mip_width /= 2;
        if (mip_height > 1)
            mip_height /= 2;
    }


    /* 将最后一个 level 的 image 的 layout 变为 shader read only */
    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout                     = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask                 = vk::AccessFlagBits::eShaderRead;
    cmd_buffer().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {},
                                 {}, {}, {barrier});


    cmd_buffer.end();
}