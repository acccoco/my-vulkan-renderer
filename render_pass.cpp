#include "./render_pass.hpp"
#include "./buffer.hpp"


vk::RenderPass create_render_pass(const Env &env)
{
    spdlog::get("logger")->info("create render pass.");


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
            .format  = env.surface_info.format.format,
            .samples = vk::SampleCountFlagBits::e1,

            /**
             * 发生在第一次访问 color/depth attchment 之前，之后
             * clear: 表示之前的内容需要被格式化为 uniform value
             * load : 表示之前的内存需要保留
             * store: 表示将内存写入 memory
             */
            .loadOp  = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,

            /**
             * 在 subpass 前后要对 stencil 做什么，这里是 color attachment，所以不需要
             * load dont care : 之前的内存不再被需要了，内容变成 undefined 状态了
             * store dont care: 内容不再被需要了，可能会被丢弃，内容变成 undefined 状态了
             */
            .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            /**
             * 在 renderpass 开始和结束的时候应该对图像使用什么 layout
             * initial: 由于不需要从 attachment 中读取数据，因此并不关心其 layout
             * final: attachment 需要交给 present engine 显示
             */
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout   = vk::ImageLayout::ePresentSrcKHR,    // 用于 swapchain
    };

    vk::AttachmentDescription depth_attach = {
            .format  = depth_format(env),
            .samples = vk::SampleCountFlagBits::e1,

            .loadOp         = vk::AttachmentLoadOp::eClear,
            .storeOp        = vk::AttachmentStoreOp::eDontCare,
            .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout   = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };

    std::vector<vk::AttachmentDescription> attachments = {
            color_attach,
            depth_attach,
    };


    /* subpass 引用的 attachment，depth 只有 1 个，color 可以有多个 */
    vk::AttachmentReference depth_attach_ref = {
            .attachment = 1,
            .layout     = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };
    std::vector<vk::AttachmentReference> color_attach_refs = {
            vk::AttachmentReference{
                    .attachment = 0,
                    .layout     = vk::ImageLayout::eColorAttachmentOptimal,
            },
    };


    std::vector<vk::SubpassDescription> subpass = {
            vk::SubpassDescription{
                    .pipelineBindPoint       = vk::PipelineBindPoint::eGraphics,
                    .colorAttachmentCount    = (uint32_t) color_attach_refs.size(),
                    .pColorAttachments       = color_attach_refs.data(),
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
                    /* dst 依赖于 src, 这里是 subpass 的 index */
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,

                    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,

                    .srcAccessMask = {},
                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                     vk::AccessFlagBits::eDepthStencilAttachmentWrite,
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


vk::Pipeline create_pipeline(const vk::Device &device, const SurfaceInfo &surface_info,
                             const vk::PipelineLayout &pipeline_layout,
                             const vk::RenderPass &render_pass)
{
    spdlog::get("logger")->info("create pipeline.");

    // 根据字节码创建 shader module
    auto create_shader_module = [&device](const std::vector<char> &code) -> vk::ShaderModule {
        return device.createShaderModule({
                // 单位是字节
                .codeSize = code.size(),
                // vector 容器可以保证转换为 uint32 后仍然是对齐的
                .pCode = reinterpret_cast<const uint32_t *>(code.data()),
        });
    };


    auto vert_shader_code               = read_file("../shader/triangle.vert.spv");
    auto frag_shader_code               = read_file("../shader/triangle.frag.spv");
    vk::ShaderModule vert_shader_module = create_shader_module(vert_shader_code);
    vk::ShaderModule frag_shader_module = create_shader_module(frag_shader_code);


    // 渲染阶段的信息
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_create_info = {
            vk::PipelineShaderStageCreateInfo{
                    .stage  = vk::ShaderStageFlagBits::eVertex,
                    .module = vert_shader_module,
                    .pName  = "main",    // 入口函数
                    .pSpecializationInfo = nullptr,    // 可以指定着色器常量的值，可以在编译期优化
            },
            vk::PipelineShaderStageCreateInfo{
                    .stage  = vk::ShaderStageFlagBits::eFragment,
                    .module = frag_shader_module,
                    .pName  = "main",
            },
    };


    // 顶点输入信息
    auto vert_bind_des = Vertex::binding_description_get();
    auto vert_attr_des = Vertex::attr_description_get();
    vk::PipelineVertexInputStateCreateInfo vertex_input_create_info_ = {
            // 指定顶点数据的信息
            .vertexBindingDescriptionCount = static_cast<uint32_t>(vert_bind_des.size()),
            .pVertexBindingDescriptions    = vert_bind_des.data(),

            // 指定顶点属性的信息
            .vertexAttributeDescriptionCount = (uint32_t) vert_attr_des.size(),
            .pVertexAttributeDescriptions    = vert_attr_des.data(),
    };


    // 如何从顶点索引得到图元
    vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info = {
            // 每三个顶点组成一个三角形，不重用顶点
            .topology = vk::PrimitiveTopology::eTriangleList,

            // 是否允许值为 0xFFFF 或 0xFFFFFFFF 索引来分解三角形
            .primitiveRestartEnable = VK_FALSE,
    };


    // viewport 配置
    vk::Viewport viewport = {
            .x        = 0.f,
            .y        = 0.f,
            .width    = (float) surface_info.extent.width,
            .height   = (float) surface_info.extent.height,
            .minDepth = 0.f,
            .maxDepth = 1.f,
    };

    /// scissor（剪刀）测试，注意和 clip maxtrix 区别开来
    /// scissor 之外的内容会被光栅化器丢弃
    /// viewport 从 NDC 变换到屏幕坐标，得到 framebuffer，然后 scissor 对 framebuffer 进行裁剪
    vk::Rect2D scissors = {
            .offset = {0, 0},
            .extent = surface_info.extent,
    };

    // 创建 viewport 的配置
    vk::PipelineViewportStateCreateInfo viewport_state = {
            .viewportCount = 1,
            .pViewports    = &viewport,
            .scissorCount  = 1,
            .pScissors     = &scissors,
    };


    // 配置光栅化阶段
    vk::PipelineRasterizationStateCreateInfo rasterization_state = {
            // 超出深度范围的深度是被 clamp 还是被丢弃
            // 需要 GPU feature
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,

            // 绘制线框/绘制顶点/绘制实心图形
            // 除了 FILL 外，其他的都需要 GPU feature
            .polygonMode = vk::PolygonMode::eFill,

            // 背面剔除
            .cullMode  = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,

            // 可以用 bias 来更改 depth，还可以基于 fragment 的斜率来更改 depth
            // 在 shadow map 中会用到
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.f,
            .depthBiasClamp          = 0.f,
            .depthBiasSlopeFactor    = 0.f,

            // 如果线宽不为 1，需要 GPU feature
            .lineWidth = 1.f,
    };


    // 多重采样：用于抗锯齿
    vk::PipelineMultisampleStateCreateInfo multisample_state = {
            .rasterizationSamples  = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 1.0f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
    };


    // 深度测试和模版测试
    vk::PipelineDepthStencilStateCreateInfo depth_stencil_info = {
            .depthTestEnable  = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp   = vk::CompareOp::eLess,
            /* 这个选项可以只显示某个深度范围的 fragment */
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable     = VK_FALSE,
    };


    /**
     * 分别为每个 color attachment 指定 blend 设置
     * 混合方法如下：https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
     * final_color.rgb = (new_color.rgb * src_blend_factor) <op> (old_color.rgb * dst_blend_factor)
     * final_color.a = (new_color.a * src_alpha_factor) <op> (old_color.a * dst_alpha_factor)
     * final_color = final_color & color_write_mask
     */
    vk::PipelineColorBlendAttachmentState color_blend_attachment = {
            .blendEnable = VK_FALSE,    // blend 的开关

            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp        = vk::BlendOp::eAdd,

            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp        = vk::BlendOp::eAdd,

            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };


    // 总的混合状态
    vk::PipelineColorBlendStateCreateInfo color_blend_state = {
            .logicOpEnable   = VK_FALSE,
            .logicOp         = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments    = &color_blend_attachment,
            .blendConstants  = {{0.f, 0.f, 0.f, 0.f}},
    };


    // 可以动态地调整 pipeline 中的某个环境，在 draw 的时候再给出具体的配置，就像 OpenGL 那样
    // 通过 VkPipelineDynamicsStateCreateInfo 来实现


    vk::GraphicsPipelineCreateInfo pipeline_create_info = {
            // shader
            .stageCount = (uint32_t) shader_stage_create_info.size(),
            .pStages    = shader_stage_create_info.data(),

            // vertex
            .pVertexInputState   = &vertex_input_create_info_,
            .pInputAssemblyState = &input_assembly_create_info,

            // pipeline fix functions
            .pViewportState      = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState   = &multisample_state,
            .pDepthStencilState  = &depth_stencil_info,
            .pColorBlendState    = &color_blend_state,
            .pDynamicState       = nullptr,

            // uniforms
            .layout = pipeline_layout,

            // render pass
            .renderPass = render_pass,
            .subpass    = 0,    // index of subpass

            /// 是否继承自其他的 pipelien
            /// 如果继承，需要在 flags 中指定
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex  = -1,
    };


    vk::Result result;
    vk::Pipeline graphics_pipeline;
    std::tie(result, graphics_pipeline) =
            device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_create_info);
    if (result != vk::Result::eSuccess)
        throw std::runtime_error("failed to create graphics pipeline.");


    // 可以安全的释放 shader module
    device.destroyShaderModule(vert_shader_module);
    device.destroyShaderModule(frag_shader_module);

    return graphics_pipeline;
}


vk::DescriptorSetLayout create_descriptor_set_layout(const vk::Device &device)
{
    spdlog::get("logger")->info("create descriptor set layout.");

    vk::DescriptorSetLayoutBinding uniform_binding = {
            .binding         = 0,
            .descriptorType  = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags      = vk::ShaderStageFlagBits::eVertex,
    };

    vk::DescriptorSetLayoutBinding sampler_binding = {
            .binding            = 1,
            .descriptorType     = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount    = 1,
            .stageFlags         = vk::ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = nullptr,
    };

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
            uniform_binding,
            sampler_binding,
    };


    return device.createDescriptorSetLayout({
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings    = bindings.data(),
    });
}


vk::PipelineLayout
create_pipelien_layout(const vk::Device &device,
                       const std::vector<vk::DescriptorSetLayout> &descriptor_set_layout)
{
    spdlog::get("logger")->info("create pipeline layout.");
    return device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount = (uint32_t) descriptor_set_layout.size(),
            .pSetLayouts    = descriptor_set_layout.data(),
    });
}


std::vector<vk::DescriptorSet>
create_descriptor_set(const vk::Device &device,
                      const vk::DescriptorSetLayout &descriptor_set_layout,
                      const vk::DescriptorPool &descriptor_pool, uint32_t frames_in_flight,
                      const std::vector<vk::Buffer> &uniform_buffer_list,
                      const vk::ImageView &tex_img_view, const vk::Sampler &tex_sampler)
{
    spdlog::get("logger")->info("create descriptor set.");

    if (uniform_buffer_list.size() != frames_in_flight)
        throw std::runtime_error("descriptor buffer count error.");

    // 为每一帧都创建一个 descriptor set
    std::vector<vk::DescriptorSet> des_set_list;
    {
        std::vector<vk::DescriptorSetLayout> layouts(frames_in_flight, descriptor_set_layout);
        des_set_list = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
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
                .dstSet          = des_set_list[i],
                .dstBinding      = 0,    // 写入 set 的哪个一 binding
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


        device.updateDescriptorSets({buffer_write, img_write}, {});
    }


    return des_set_list;
}
