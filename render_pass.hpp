#pragma once

#include "./include_vk.hpp"
#include "./device.hpp"
#include "./tools.hpp"
#include "./vertex.hpp"


inline vk::RenderPass create_render_pass(const vk::Device &device, const SurfaceInfo &surface_info)
{
    spdlog::get("logger")->info("create render pass.");
    std::vector<vk::AttachmentDescription> color_attachment = {
            vk::AttachmentDescription{
                    .format  = surface_info.format.format,
                    .samples = vk::SampleCountFlagBits::e1,

                    // 发生在第一次访问 color/depth attchment 之前，之后
                    .loadOp  = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eStore,

                    // 在 subpass 前后要对 stencil 做什么，这里是 color attachment，所以不需要
                    .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

                    /// 在 renderpass 开始和结束的时候应该对图像使用什么 layout
                    /// 由于不需要从 attachment 中读取数据，因此并不关心其 layout，驱动可以做一些优化
                    /// subpass 之后，attachment 需要交给 present engine 显示，因此设为 engine 需要的 layout
                    .initialLayout = vk::ImageLayout::eUndefined,
                    .finalLayout   = vk::ImageLayout::ePresentSrcKHR,    // 用于 swapchain
            },
    };


    // subpass 引用 color attachment
    std::vector<vk::AttachmentReference> color_attachment_ref = {
            vk::AttachmentReference{
                    .attachment = 0,    // index in render pass

                    // 当前 subpass 需要 attachment 是什么样的 layout，由于之前的 attachment description 中
                    // 已经说明了 initial layout 是 undefined 的，因此驱动很容易优化
                    .layout = vk::ImageLayout::eColorAttachmentOptimal,
            },
    };


    std::vector<vk::SubpassDescription> subpass = {
            vk::SubpassDescription{
                    .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                    // layout(location = 0) out vec4 color;
                    .colorAttachmentCount = (uint32_t) color_attachment_ref.size(),
                    .pColorAttachments    = color_attachment_ref.data(),
            },
    };


    /// dst subpass 依赖于 src subpass
    /// 非常好的解答：https://www.reddit.com/r/vulkan/comments/s80reu/subpass_dependencies_what_are_those_and_why_do_i/
    std::vector<vk::SubpassDependency> dependency = {
            vk::SubpassDependency{
                    /// VK_SUBPASS_EXTERNAL 表示 dst subpass 依赖于 之前的 renderpass 中的所有 subpass
                    .srcSubpass    = VK_SUBPASS_EXTERNAL,
                    .dstSubpass    = 0,
                    .srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    .srcAccessMask = {},
                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            },
    };


    vk::RenderPassCreateInfo render_pass_info = {
            .attachmentCount = (uint32_t) color_attachment.size(),
            .pAttachments    = color_attachment.data(),
            .subpassCount    = (uint32_t) subpass.size(),
            .pSubpasses      = subpass.data(),
            .dependencyCount = (uint32_t) dependency.size(),
            .pDependencies   = dependency.data(),
    };

    return device.createRenderPass(render_pass_info);
}


inline vk::Pipeline create_pipeline(const vk::Device &device, const SurfaceInfo &surface_info,
                                    const vk::PipelineLayout &pipeline_layout, const vk::RenderPass &render_pass)
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
                    .stage               = vk::ShaderStageFlagBits::eVertex,
                    .module              = vert_shader_module,
                    .pName               = "main",     // 入口函数
                    .pSpecializationInfo = nullptr,    // 可以指定着色器常量的值，可以在编译期优化
            },
            vk::PipelineShaderStageCreateInfo{
                    .stage  = vk::ShaderStageFlagBits::eFragment,
                    .module = frag_shader_module,
                    .pName  = "main",
            },
    };


    // 顶点输入信息
    auto vert_bind_des                                               = Vertex::get_binding_description_();
    auto vert_attr_des                                               = Vertex::get_attr_descriptions_();
    vk::PipelineVertexInputStateCreateInfo vertex_input_create_info_ = {
            // 指定顶点数据的信息
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions    = &vert_bind_des,

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


    // 深度测试和模版测试：暂不需要


    /// 分别为每个 color attachment 指定 blend 设置
    /// 混合方法如下：https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
    /// final_color.rgb = (new_color.rgb * src_blend_factor) <op> (old_color.rgb * dst_blend_factor)
    /// final_color.a = (new_color.a * src_alpha_factor) <op> (old_color.a * dst_alpha_factor)
    /// final_color = final_color & color_write_mask
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
            .pDepthStencilState  = nullptr,
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
    std::tie(result, graphics_pipeline) = device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_create_info);
    if (result != vk::Result::eSuccess)
        throw std::runtime_error("failed to create graphics pipeline.");


    // 可以安全的释放 shader module
    device.destroyShaderModule(vert_shader_module);
    device.destroyShaderModule(frag_shader_module);

    return graphics_pipeline;
}


inline vk::DescriptorSetLayout create_descriptor_set_layout(const vk::Device &device)
{
    spdlog::get("logger")->info("create descriptor set layout.");
    vk::DescriptorSetLayoutBinding bingding = {
            .binding         = 0,
            .descriptorType  = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags      = vk::ShaderStageFlagBits::eVertex,
    };


    return device.createDescriptorSetLayout({
            .bindingCount = 1,
            .pBindings    = &bingding,
    });
}


inline vk::PipelineLayout create_pipelien_layout(const vk::Device &device,
                                                 const std::vector<vk::DescriptorSetLayout> &descriptor_set_layout)
{
    spdlog::get("logger")->info("create pipeline layout.");
    return device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount = (uint32_t) descriptor_set_layout.size(),
            .pSetLayouts    = descriptor_set_layout.data(),
    });
}