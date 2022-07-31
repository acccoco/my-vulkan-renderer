#include "../render_pass.hpp"
#include "profile.hpp"


/**
 * 创建 render pass，主要包含：attachement，subpass，subpass dependency
 */
vk::RenderPass render_pass_create(const FramebufferLayout_temp &framebuffer_layout)
{
    LogStatic::logger()->info("create render pass.");
    auto env = *EnvSingleton::env();


    /**
     * 关于 attachment 的 layout 和 content 的理解：
     *
     * attachment description 规定了 attachment 的 content 在 load 和 store 时应该被如何对待；
     * 还规定了 attachment 的初始 layout 和最终 layout
     *
     * attachment reference 指定了 subpass 需要的 attachment 是什么 layout
     * attachment 会发生 automic layout transition，dependency 会影响 transition 发生的时机
     *
     * subpass 之间可以并行，dependency 可以控制 subpass 的运行 synchronization
     * dependency 实际上是一种 memory dependency
     */


    vk::AttachmentDescription color_attach = {
            .format  = framebuffer_layout.color_format,
            .samples = framebuffer_layout.color_sample,

            /**
             * 发生在第一次访问 color/depth attchment 之前/之后
             * [load]  clear: 表示之前的内容需要被格式化为 uniform value
             * [load]  load : 表示之前的内存需要保留
             * [store] store: 表示将内存写入 memory
             * [load]  dont care : 之前的内存不再被需要了，内容变成 undefined 状态了
             * [store] dont care: 内容不再被需要了，可能会被丢弃，内容变成 undefined 状态了
             */
            .loadOp         = vk::AttachmentLoadOp::eClear,
            .storeOp        = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            /**
             * 在 renderpass 开始和结束的时候应该对图像使用什么 layout
             * [initial] undefined: 由于不需要从 attachment 中读取数据，因此并不关心其 layout
             * [final]   color attach optimal: MSAA 的 render target，不能直接用于 present 到 surface
             */
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout   = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentDescription depth_attach = {
            .format  = framebuffer_layout.depth_format,
            .samples = framebuffer_layout.depth_sample,

            .loadOp         = vk::AttachmentLoadOp::eClear,
            .storeOp        = vk::AttachmentStoreOp::eDontCare,
            .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout   = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };

    vk::AttachmentDescription resolve_attach = {
            .format         = framebuffer_layout.resolve_format,
            .samples        = framebuffer_layout.resolve_sample,
            .loadOp         = vk::AttachmentLoadOp::eDontCare,
            .storeOp        = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout  = vk::ImageLayout::eUndefined,
            .finalLayout    = vk::ImageLayout::ePresentSrcKHR,
    };

    std::vector<vk::AttachmentDescription> attachments = {
            color_attach,
            depth_attach,
            resolve_attach,
    };


    /* subpass 引用的 attachment，depth 只有 1 个，color 可以有多个 */
    std::vector<vk::AttachmentReference> color_attach_refs = {
            vk::AttachmentReference{
                    .attachment = 0,
                    .layout     = vk::ImageLayout::eColorAttachmentOptimal,
            },
    };
    vk::AttachmentReference depth_attach_ref = {
            .attachment = 1,
            .layout     = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };
    vk::AttachmentReference resolve_attach_ref = {
            .attachment = 2,
            .layout     = vk::ImageLayout::eColorAttachmentOptimal,
    };


    std::vector<vk::SubpassDescription> subpass = {
            vk::SubpassDescription{
                    .pipelineBindPoint       = vk::PipelineBindPoint::eGraphics,
                    .colorAttachmentCount    = (uint32_t) color_attach_refs.size(),
                    .pColorAttachments       = color_attach_refs.data(),
                    .pResolveAttachments     = &resolve_attach_ref,
                    .pDepthStencilAttachment = &depth_attach_ref,
            },
    };


    /**
     * dst subpass 依赖于 src subpass，可以参考下面的回答
     * https://www.reddit.com/r/vulkan/comments/s80reu/subpass_dependencies_what_are_those_and_why_do_i/
     * https://www.youtube.com/watch?v=x2SGVjlVGhE
     */
    std::vector<vk::SubpassDependency> dependency = {
            vk::SubpassDependency{
                    /**
                     * 不允许隐式的 layout transition，因为其 srcStage = TOP, srcAccess = 0，可能导致 data race
                     * 这里 dependency 是一个 execution barrier，表示 subpass#0 需要等待上一帧的 color 和 depth 都用完
                     * 由于不需要上一帧的数据，所以 srcAccess = 0
                     * 上一帧的 color 和 depth stage 完成后，由于 barrier，会执行 layout transition，然后当前帧的 subpass#0 的
                     *      depth 和 color stage 可以执行了
                     */
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,

                    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput
                                  | vk::PipelineStageFlagBits::eLateFragmentTests,
                    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput
                                  | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .srcAccessMask = {},
                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
                                   | vk::AccessFlagBits::eDepthStencilAttachmentWrite
                                   | vk::AccessFlagBits::eDepthStencilAttachmentRead,
            },
    };


    vk::RenderPassCreateInfo render_pass_info = {
            .attachmentCount = (uint32_t) attachments.size(),
            .pAttachments    = attachments.data(),

            .subpassCount = (uint32_t) subpass.size(),
            .pSubpasses   = subpass.data(),

            .dependencyCount = (uint32_t) dependency.size(),
            .pDependencies   = dependency.data(),
    };
    return env.device.createRenderPass(render_pass_info);
}


vk::Pipeline pipeline_create(const vk::PipelineLayout &pipeline_layout,
                             const vk::RenderPass &render_pass)
{
    LogStatic::logger()->info("create pipeline.");
    auto env = EnvSingleton::env();


    /* 根据字节码创建 shader module */
    auto shader_module_create = [](const std::string &file_path) -> vk::ShaderModule {
        std::vector<char> code          = read_file(file_path);
        vk::ShaderModuleCreateInfo info = {
                .codeSize = code.size(), /* 单位是字节 */
                // vector 容器可以保证转换为 uint32 后仍然是对齐的
                .pCode = reinterpret_cast<const uint32_t *>(code.data()),
        };
        return EnvSingleton::env()->device.createShaderModule(info);
    };
    vk::ShaderModule vert_shader_module = shader_module_create(SHADER("triangle.vert.spv"));
    vk::ShaderModule frag_shader_module = shader_module_create(SHADER( "/triangle.frag.spv"));
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_create_info = {
            vk::PipelineShaderStageCreateInfo{
                    .stage  = vk::ShaderStageFlagBits::eVertex,
                    .module = vert_shader_module,
                    .pName  = "main",    // 入口函数
            },
            vk::PipelineShaderStageCreateInfo{
                    .stage  = vk::ShaderStageFlagBits::eFragment,
                    .module = frag_shader_module,
                    .pName  = "main",
            },
    };


    /* vertex buffer 以及 vertex attribute 的信息 */
    auto vert_bind_description = Vertex::binding_description_get();
    auto vert_attr_description = Vertex::attr_description_get();
    vk::PipelineVertexInputStateCreateInfo vertex_input_create_info_ = {
            /* vertex buffer 的信息 */
            .vertexBindingDescriptionCount = static_cast<uint32_t>(vert_bind_description.size()),
            .pVertexBindingDescriptions    = vert_bind_description.data(),

            /* vertex attribute 的信息 */
            .vertexAttributeDescriptionCount = (uint32_t) vert_attr_description.size(),
            .pVertexAttributeDescriptions    = vert_attr_description.data(),
    };


    /* 图元装配的信息 */
    vk::PipelineInputAssemblyStateCreateInfo assembly_info = {
            /* 每三个顶点组成一个三角形，不重用顶点 */
            .topology = vk::PrimitiveTopology::eTriangleList,

            /* 是否允许值为 0xFFFF 或 0xFFFFFFFF 索引来分解三角形 */
            .primitiveRestartEnable = VK_FALSE,
    };


    /**
     * viewport 配置
     * scissor 测试发生在 viewport 变换之后，未通过的会被丢弃
     * NDC -> framebuffer -> scissor test
     */
    vk::Viewport viewport = {
            .x        = 0.f,
            .y        = 0.f,
            .width    = (float) env->present_extent.width,
            .height   = (float) env->present_extent.height,
            .minDepth = 0.f,
            .maxDepth = 1.f,
    };
    vk::Rect2D scissors = {
            .offset = {0, 0},
            .extent = env->present_extent,
    };
    vk::PipelineViewportStateCreateInfo viewport_state = {
            .viewportCount = 1,
            .pViewports    = &viewport,
            .scissorCount  = 1,
            .pScissors     = &scissors,
    };


    /* 光栅化阶段 */
    vk::PipelineRasterizationStateCreateInfo rasterization_state = {
            /* 超出深度范围的深度是被 clamp 还是被丢弃，需要 GPU feature */
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,

            /* 绘制线框/绘制顶点/绘制实心图形（除了 FILL 外，其他的都需要 GPU feature） */
            .polygonMode = vk::PolygonMode::eFill,

            /* 背面剔除，发生在 viewport 变换之后 */
            .cullMode  = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            /* 基于 bias 或 fragment 的斜率来更改 depth（在 shadow map 中会用到） */
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.f,
            .depthBiasClamp          = 0.f,
            .depthBiasSlopeFactor    = 0.f,

            /* 如果线宽不为 1，需要 GPU feature */
            .lineWidth = 1.f,
    };


    /* MSAA */
    vk::PipelineMultisampleStateCreateInfo multisample_state = {
            /* 使用最高的 MSAA */
            .rasterizationSamples  = EnvSingleton::max_sample_cnt(),
            .sampleShadingEnable   = VK_TRUE,
            .minSampleShading      = .2f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
    };


    /* 深度测试和模版测试 */
    vk::PipelineDepthStencilStateCreateInfo depth_stencil_info = {
            .depthTestEnable  = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp   = vk::CompareOp::eLess,
            /* 这个选项可以只显示某个深度范围的 fragment */
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable     = VK_FALSE,
    };


    /* blend：需要分别为每个 color attachment 指定 blend 设置 */
    vk::PipelineColorBlendAttachmentState color_blend_attachment = {
            .blendEnable = VK_FALSE, /* blend 的开关 */

            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp        = vk::BlendOp::eAdd,

            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp        = vk::BlendOp::eAdd,

            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    vk::PipelineColorBlendStateCreateInfo color_blend_state = {
            .logicOpEnable   = VK_FALSE,
            .logicOp         = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments    = &color_blend_attachment,
            .blendConstants  = {{0.f, 0.f, 0.f, 0.f}},
    };


    /* 创建 pipeline */
    vk::GraphicsPipelineCreateInfo pipeline_create_info = {
            /* shader stages */
            .stageCount = (uint32_t) shader_stage_create_info.size(),
            .pStages    = shader_stage_create_info.data(),

            /* vertex */
            .pVertexInputState   = &vertex_input_create_info_,
            .pInputAssemblyState = &assembly_info,

            /* pipeline 的固定功能 */
            .pViewportState      = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState   = &multisample_state,
            .pDepthStencilState  = &depth_stencil_info,
            .pColorBlendState    = &color_blend_state,
            .pDynamicState       = nullptr,

            /* uniform 相关 */
            .layout = pipeline_layout,

            /* 绑定到哪个 render pass */
            .renderPass = render_pass,
            .subpass    = 0, /* index of subpass */

            /* 是否继承自其他的 pipelien。如果继承，需要在 flags 中指定 */
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex  = -1,
    };
    vk::Result result;
    vk::Pipeline graphics_pipeline;
    std::tie(result, graphics_pipeline) =
            env->device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_create_info);
    if (result != vk::Result::eSuccess)
        throw std::runtime_error("failed to create graphics pipeline.");


    /* 释放临时资源 */
    env->device.destroyShaderModule(vert_shader_module);
    env->device.destroyShaderModule(frag_shader_module);

    return graphics_pipeline;
}


vk::DescriptorSetLayout descriptor_set_layout_create()
{
    LogStatic::logger()->info("create descriptor set layout.");


    vk::DescriptorSetLayoutBinding uniform_binding = {
            .binding         = 0,
            .descriptorType  = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1, /* 大于 1 表示数组 */
            .stageFlags      = vk::ShaderStageFlagBits::eVertex,
    };
    vk::DescriptorSetLayoutBinding sampler_binding = {
            .binding            = 1,
            .descriptorType     = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount    = 1,
            .stageFlags         = vk::ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = nullptr,
    };


    /**
     * 这个 layout 表示：
     * layout(binding = 0) uniform UniformBlock;
     * layout(binding = 1) uniform sampler2D;
     */
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
            uniform_binding,
            sampler_binding,
    };
    return EnvSingleton::env()->device.createDescriptorSetLayout({
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings    = bindings.data(),
    });
}


vk::PipelineLayout
pipeline_layout_create(const std::vector<vk::DescriptorSetLayout> &descriptor_set_layout)
{
    LogStatic::logger()->info("create pipeline layout.");

    return EnvSingleton::env()->device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount = (uint32_t) descriptor_set_layout.size(),
            .pSetLayouts    = descriptor_set_layout.data(),
    });
}


/**
 * 为每个 frames inflight 创建一个 descriptor set；向 set 内写入 sampler 和 uniform block
 */
std::vector<vk::DescriptorSet>
create_descriptor_set(const vk::DescriptorSetLayout &descriptor_set_layout,
                      const vk::DescriptorPool &descriptor_pool, uint32_t frames_in_flight,
                      std::array<vk::Buffer, 2U> uniform_buffer_list,
                      const vk::ImageView &tex_img_view, const vk::Sampler &tex_sampler)
{
    LogStatic::logger()->info("create descriptor set.");
    auto env = EnvSingleton::env();


    if (uniform_buffer_list.size() != frames_in_flight)
        throw std::runtime_error("descriptor buffer count error.");

    // 为每一帧都创建一个 descriptor set
    std::vector<vk::DescriptorSet> des_set_list;
    {
        std::vector<vk::DescriptorSetLayout> layouts(frames_in_flight, descriptor_set_layout);
        des_set_list = env->device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
                .descriptorPool     = descriptor_pool,
                .descriptorSetCount = (uint32_t) layouts.size(),
                .pSetLayouts        = layouts.data(),
        });
    }


    for (size_t i = 0; i < frames_in_flight; ++i)
    {
        vk::DescriptorBufferInfo buffer_info = {
                .buffer = uniform_buffer_list[i],
                .offset = 0,
                .range  = sizeof(UniformBufferObject),
        };
        vk::WriteDescriptorSet buffer_write = {
                .dstSet     = des_set_list[i],
                .dstBinding = 0,         // 写入 set 的哪个一 binding
                .dstArrayElement = 0,    // 如果 binding 对应数组，从第几个元素开始写
                .descriptorCount = 1,    // 写入几个数组元素
                .descriptorType  = vk::DescriptorType::eUniformBuffer,

                // buffer, image, image view 三选一
                .pBufferInfo = &buffer_info,
        };


        vk::DescriptorImageInfo img_info = {
                .sampler     = tex_sampler,
                .imageView   = tex_img_view,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        vk::WriteDescriptorSet img_write = {
                .dstSet          = des_set_list[i],
                .dstBinding      = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo      = &img_info,
        };


        env->device.updateDescriptorSets({buffer_write, img_write}, {});
    }


    return des_set_list;
}
