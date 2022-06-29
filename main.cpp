#include <set>
#include <sstream>
#include <limits>
#include <cassert>
#include <algorithm>

#include "./main.h"
#include "./tools.h"


// TODO 使用 cpp 版本的 vulkan
// 参考：https://github.com/KhronosGroup/Vulkan-Hpp/，里面有很多的 samples，对学习 vulkan 也很有用的


int main()
{
    init_spdlog();

    Application app{};

    try
    {
        app.run();
    } catch (const std::exception &e)
    {
        SPDLOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


void init_spdlog()
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%^%L%$]%v");
}


VkSurfaceFormatKHR Application::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &formats)
{
    for (const auto &format: formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }

    // 如果每个都不合格，那么就选择第一个
    return formats[0];
}


VkPresentModeKHR Application::choose_swap_present_model(const std::vector<VkPresentModeKHR> &present_modes)
{
    for (const auto &present_mode: present_modes)
    {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_mode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}


VkExtent2D Application::choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities)
{
    /**
     * vulkan 使用的是以 pixel 为单位的分辨率，glfw 初始化窗口时使用的是 screen coordinate 为单位的分辨率
     * 在 Apple Retina display 上，pixel 是 screen coordinate 的 2 倍
     * 最好的方法就是使用 glfwGetFramebufferSize 去查询 window 以 pixel 为单位的大小
     */
    assert(_window != nullptr);

    // numeric_limits::max 的大小表示自适应大小
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);

    VkExtent2D acture_extent = {(uint32_t) width, (uint32_t) height};
    acture_extent.width =
            std::clamp(acture_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    acture_extent.height =
            std::clamp(acture_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return acture_extent;
}


void Application::create_swap_chain()
{
    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(_physical_device_info.device_surface_format_list);
    VkPresentModeKHR present_mode     = choose_swap_present_model(_physical_device_info.device_surface_present_mode);
    _swapchain_extent                 = choose_swap_extent(_physical_device_info.device_surface_capabilities);
    _swapchain_iamge_format           = surface_format.format;

    /// vulkan 规定，minImageCount 至少是 1
    /// maxImageCount 为 0 时表示没有限制
    uint32_t image_cnt = _physical_device_info.device_surface_capabilities.minImageCount + 1;
    if (_physical_device_info.device_surface_capabilities.maxImageCount > 0 &&
        image_cnt > _physical_device_info.device_surface_capabilities.maxImageCount)
        image_cnt = _physical_device_info.device_surface_capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR create_info = {
            .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface         = _surface,     // to present image on
            .minImageCount   = image_cnt,    // may create more presentable images
            .imageFormat     = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent     = _swapchain_extent,    // image extent, size in pixel

            // number of views, for non-stereoscopic app, it is 1
            .imageArrayLayers = 1,

            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

            // 是否需要在 present 前对 image 做一些变换，currentTransform 意味着不用任何变换
            .preTransform = _physical_device_info.device_surface_capabilities.currentTransform,

            // 如果一个 window 中有多个 surface，是否根据 alpha 值与其他 surface 进行复合
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

            .presentMode = present_mode,    // eg. 三缓
            .clipped     = VK_TRUE,         // 是否丢弃 surface 不可见区域的渲染操作，可提升性能

            // 窗口 resize 的时候，之前的 swap chain 会失效，暂不考虑这种情况
            .oldSwapchain = VK_NULL_HANDLE,
    };

    /// queueFamilyIndexCount 表示有多少个 queue family 会访问 swapchain 的 images
    /// CONCURRENT 表示 image 可以被多个 queue family 访问，性能相对会低一些
    /// EXCLUSIVE 表示 image 只会被一个 queue family 访问
    /// image 位于 graphics queue family 时，可以进行绘图
    /// image 位于 present queue family 时，可以进行显示
    /// graphics 和 present 可能是同一个 queue family，这时就不需要进行 share 了
    uint32_t queue_family_indices[] = {_physical_device_info.graphics_queue_family_idx.value(),
                                       _physical_device_info.present_queue_family_idx.value()};
    if (_physical_device_info.graphics_queue_family_idx != _physical_device_info.present_queue_family_idx)
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT, create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else
    {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    if (vkCreateSwapchainKHR(_device, &create_info, nullptr, &_swapchain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain.");

    // 获取 swap chain 里面的 image，因为 swapchain 可能会创建更多的 image，所以需要重新查询数量
    vkGetSwapchainImagesKHR(_device, _swapchain, &image_cnt, nullptr);
    _swapchain_image_list.resize(image_cnt);
    vkGetSwapchainImagesKHR(_device, _swapchain, &image_cnt, _swapchain_image_list.data());
}


void Application::create_pipeline()
{
    assert(_device != VK_NULL_HANDLE);
    assert(_render_pass != VK_NULL_HANDLE);

    auto vert_shader_code = read_file("../shader/triangle.vert.spv");
    auto frag_shader_code = read_file("../shader/triangle.frag.spv");

    // 根据字节码创建 shader module
    auto create_shader_module = [this](const std::vector<char> &code) -> VkShaderModule {
        VkShaderModuleCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                // 单位是字节
                .codeSize = code.size(),
                // vector 容器可以保证内存对齐
                .pCode = reinterpret_cast<const uint32_t *>(code.data()),
        };

        VkShaderModule shader_module;
        if (vkCreateShaderModule(_device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
            throw std::runtime_error("failed to create shader module.");

        return shader_module;
    };

    VkShaderModule vert_shader_module = create_shader_module(vert_shader_code);
    VkShaderModule frag_shader_module = create_shader_module(frag_shader_code);

    // 渲染阶段的信息
    VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {
            VkPipelineShaderStageCreateInfo{
                    .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage               = VK_SHADER_STAGE_VERTEX_BIT,
                    .module              = vert_shader_module,
                    .pName               = "main",     // 入口函数
                    .pSpecializationInfo = nullptr,    // 可以指定着色器常量的值，可以在编译期优化
            },
            VkPipelineShaderStageCreateInfo{
                    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = frag_shader_module,
                    .pName  = "main",
            },
    };

    // 顶点输入信息
    auto vertex_binding_description                               = Vertex::get_binding_description();
    auto vertex_attribute_description                             = Vertex::get_attribute_descripions();
    VkPipelineVertexInputStateCreateInfo vertex_input_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            // 指定顶点数据的信息
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions    = &vertex_binding_description,
            // 指定顶点属性的信息
            .vertexAttributeDescriptionCount = (uint32_t) vertex_attribute_description.size(),
            .pVertexAttributeDescriptions    = vertex_attribute_description.data(),
    };

    // 如何从顶点索引得到图元
    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {
            // 每三个顶点组成一个三角形，不重用顶点
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,

            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,

            // 是否允许值为 0xFFFF 或 0xFFFFFFFF 索引来分解三角形
            .primitiveRestartEnable = VK_FALSE,
    };

    // viewport 配置
    VkViewport viewport = {
            .x        = 0.f,
            .y        = 0.f,
            .width    = (float) _swapchain_extent.width,
            .height   = (float) _swapchain_extent.height,
            .minDepth = 0.f,
            .maxDepth = 1.f,
    };

    /// scissor（剪刀）测试，注意和 clip maxtrix 区别开来
    /// scissor 之外的内容会被光栅化器丢弃
    /// viewport 从 NDC 变换到屏幕坐标，得到 framebuffer，然后 scissor 对 framebuffer 进行裁剪
    VkRect2D scissors = {
            .offset = {0, 0},
            .extent = _swapchain_extent,
    };

    // 创建 viewport 的配置
    VkPipelineViewportStateCreateInfo viewport_state = {
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports    = &viewport,
            .scissorCount  = 1,
            .pScissors     = &scissors,
    };

    // 配置光栅化阶段
    VkPipelineRasterizationStateCreateInfo rasterization_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,

            // 超出深度范围的深度是被 clamp 还是被丢弃
            // 需要 GPU feature
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,

            // 绘制线框/绘制顶点/绘制实心图形
            // 除了 FILL 外，其他的都需要 GPU feature
            .polygonMode = VK_POLYGON_MODE_FILL,

            // 背面剔除
            .cullMode  = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,

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
    VkPipelineMultisampleStateCreateInfo multisample_state = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
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
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
            .blendEnable = VK_FALSE,    // blend 的开关

            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp        = VK_BLEND_OP_ADD,

            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD,

            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
    };

    // 总的混合状态
    VkPipelineColorBlendStateCreateInfo color_blend_state = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments    = &color_blend_attachment,
            .blendConstants  = {0.f, 0.f, 0.f, 0.f},
    };

    // 可以动态地调整 pipeline 中的某个环境，在 draw 的时候再给出具体的配置，就像 OpenGL 那样
    // 通过 VkPipelineDynamicsStateCreateInfo 来实现

    // uniform 变量的指定
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = 0,
            .pSetLayouts            = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges    = nullptr,
    };
    if (vkCreatePipelineLayout(_device, &pipeline_layout_create_info, nullptr, &_pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout!");

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

            // shader
            .stageCount = 2,
            .pStages    = shader_stage_create_info,

            // vertex
            .pVertexInputState   = &vertex_input_create_info,
            .pInputAssemblyState = &input_assembly_create_info,

            // pipeline 流程
            .pViewportState      = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState   = &multisample_state,
            .pDepthStencilState  = nullptr,
            .pColorBlendState    = &color_blend_state,
            .pDynamicState       = nullptr,

            // uniforms
            .layout = _pipeline_layout,

            // render pass
            .renderPass = _render_pass,
            .subpass    = 0,    // index of subpass in render pass

            // 是否继承自其他的 pipeline
            // 需要在 flags 中开启 VkGraphicsPipelineCreateInfo
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex  = -1,
    };

    if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &_graphics_pipeline) !=
        VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline!");

    // 可以安全的释放 shader module
    vkDestroyShaderModule(_device, vert_shader_module, nullptr);
    vkDestroyShaderModule(_device, frag_shader_module, nullptr);
}


void Application::create_framebuffers()
{
    assert(_device != VK_NULL_HANDLE);

    _swapchain_framebuffer_list.resize(_swapchain_image_view_list.size());
    for (size_t i = 0; i < _swapchain_image_view_list.size(); ++i)
    {
        VkImageView attachments[] = {
                _swapchain_image_view_list[i],
        };
        VkFramebufferCreateInfo framebuffer_create_info = {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass      = _render_pass,
                .attachmentCount = 1,
                .pAttachments    = attachments,
                .width           = _swapchain_extent.width,
                .height          = _swapchain_extent.height,
                .layers          = 1,
        };

        if (vkCreateFramebuffer(_device, &framebuffer_create_info, nullptr, &_swapchain_framebuffer_list[i]) !=
            VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }
}


void Application::create_image_views()
{
    assert(_device != VK_NULL_HANDLE);

    _swapchain_image_view_list.resize(_swapchain_image_list.size());
    for (uint32_t i = 0; i < _swapchain_image_list.size(); i++)
    {
        VkImageViewCreateInfo create_info = {
                .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image    = _swapchain_image_list[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format   = _swapchain_iamge_format,

                // 每个通道的映射方式：例如可以将 alpha 通道映射为一个常数
                .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                               .a = VK_COMPONENT_SWIZZLE_IDENTITY},

                .subresourceRange = {.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel   = 0,
                                     .levelCount     = 1,
                                     .baseArrayLayer = 0,
                                     // if VR, then layer count greater than 1
                                     .layerCount = 1},
        };
        if (vkCreateImageView(_device, &create_info, nullptr, &_swapchain_image_view_list[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create image view.");
    }
}

void Application::run()
{
    print_instance_info();
    init_window();
    init_vulkan();
    mainLoop();
    cleanup();
}


void Application::init_window()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    _window = glfwCreateWindow((int) WIDTH, (int) HEIGHT, "Vulkan", nullptr, nullptr);

    // 设置窗口大小改变的 callback
    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, [](GLFWwindow *window, int width, int height) {
        auto app                  = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
        app->_framebuffer_resized = true;
    });
}


void Application::print_instance_info()
{
    // vk instance 支持的 extension
    {
        /// 首先获取可用的 extension 数量
        uint32_t ext_cnt = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_cnt, nullptr);
        // 查询 vk 能够支持的扩展的细节
        std::vector<VkExtensionProperties> ext_list(ext_cnt);
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_cnt, ext_list.data());
        // 打印出扩展
        std::stringstream ss;
        ss << "instance extensions(available)(" << ext_cnt << "): \n";
        for (const auto &ext: ext_list)
            ss << "\t" << ext.extensionName << "\n";
        SPDLOG_INFO("{}", ss.str());
    }
}


void Application::init_vulkan()
{
    if (!check_instance_layers())
        throw std::runtime_error("validation layer required, but not available.");
    create_instance();

    /// 初始化 vulkan hpp 的默认 dispatcher
    /// 有了这个，就不用手动查询扩展函数的地址了
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);

    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swap_chain();
    create_image_views();
    create_render_pass();
    create_pipeline();
    create_framebuffers();
    create_command_pool();
    create_vertex_buffer_();
    create_index_buffer();
    create_frame_synchro_data();
}


void Application::create_instance()

{
    // （这是可选的） 关于应用程序的信息，驱动可以根据这些信息对应用程序进行一些优化
    VkApplicationInfo app_info = {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,    // 这个结构体的类型
            .pApplicationName   = "vk app",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_0,
    };

    // 所需的 vk 扩展
    std::vector<const char *> required_extensions = get_instance_ext();

    // 这些数据用于指定当前应用程序需要的全局 extension 以及 validation layers
    VkInstanceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,

            /// 一般情况下 debug messenger 只有在 instance 创建后和销毁前才是有效的，
            /// 将这个 create info 传递给 instance 创建信息的 pNext 字段，
            /// 可以在 instance 的创建和销毁过程进行 debug
            .pNext = &_debug_messenger_create_info,

            /// 表示 vulkan 除了枚举出默认的 physical device 外，
            /// 还会枚举出符合 vulkan 可移植性的 physical device
            /// 基于 metal 的 vulkan 需要这个
            .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,

            .pApplicationInfo = &app_info,

            // layers
            .enabledLayerCount   = (uint32_t) (_instance_layer_list.size()),
            .ppEnabledLayerNames = _instance_layer_list.data(),

            // extensions
            .enabledExtensionCount   = (uint32_t) required_extensions.size(),
            .ppEnabledExtensionNames = required_extensions.data(),
    };

    // 创建 vk 的 instance
    if (vkCreateInstance(&create_info, nullptr, &_instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create vk instance.");
}


bool Application::check_instance_layers()

{
    // layers 的数量
    uint32_t layer_cnt = 0;
    vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr);

    // 当前系统支持的 layer
    std::vector<VkLayerProperties> supported_layer_list(layer_cnt);
    vkEnumerateInstanceLayerProperties(&layer_cnt, supported_layer_list.data());
    std::stringstream log_ss;
    log_ss << "instance layers(available)(" << layer_cnt << "):\n";
    for (const auto &layer: supported_layer_list)
        log_ss << "\t" << layer.layerName << ": " << layer.description << "\n";
    SPDLOG_INFO("{}", log_ss.str());

    // 检查需要的 layer 是否受支持（笛卡尔积操作）
    for (const char *layer_needed: _instance_layer_list)
    {
        bool layer_found = false;
        for (const auto &layer_supported: supported_layer_list)
        {
            if (strcmp(layer_needed, layer_supported.layerName) == 0)
            {
                layer_found = true;
                break;
            }
        }
        if (!layer_found)
            return false;
    }
    return true;
}


void Application::create_surface()
{
    assert(_instance != VK_NULL_HANDLE);

    // 调用 glfw 来创建 window surface，这样可以避免平台相关的细节
    if (glfwCreateWindowSurface(_instance, _window, nullptr, &_surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create widnow surface by glfw.");
}


std::vector<const char *> Application::get_instance_ext()
{
    std::vector<const char *> ext_list;

    /// glfw 所需的 vk 扩展，window interface 相关的扩展
    /// VK_KHR_surface: 这个扩展可以暴露出 VkSurfaceKHR 对象，glfw 可以读取这个对象
    uint32_t glfw_ext_cnt = 0;
    const char **glfw_ext_list;
    glfw_ext_list = glfwGetRequiredInstanceExtensions(&glfw_ext_cnt);
    for (int i = 0; i < glfw_ext_cnt; ++i)
        ext_list.push_back(glfw_ext_list[i]);

    // validation layer 需要的扩展
    ext_list.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // 基于 metal API 的 vulkan 实现需要这些扩展
    ext_list.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    ext_list.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    // 所有的扩展
    std::stringstream log_ss;
    log_ss << "instance extensions(required)(" << ext_list.size() << "): \n";
    for (const char *ext: ext_list)
        log_ss << "\t" << ext << "\n";
    SPDLOG_INFO("{}", log_ss.str());
    return ext_list;
}


void Application::setup_debug_messenger()
{
    // 这个函数需要在 instance 已经创建之后执行
    assert(_instance != VK_NULL_HANDLE);

    // 因为 vkCreateDebugUtilsMessengerEXT 是扩展函数，因此需要查询函数指针
    auto vkCreateDebugUtilsMessengerEXT =
            (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT == nullptr)
        throw std::runtime_error("failed to find function(vkCreateDebugUtilsMessengerEXT)");

    // 为 instance 设置 debug messenger
    if (vkCreateDebugUtilsMessengerEXT(_instance, &_debug_messenger_create_info, nullptr, &_debug_messenger) !=
        VK_SUCCESS)
        throw std::runtime_error("failed to setup debug messenger.");
}


VkBool32 Application::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                     VkDebugUtilsMessageTypeFlagsEXT message_type,
                                     const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *)
{

    const char *type;
    switch (message_type)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: type = "Gene"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: type = "Vali"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: type = "Perf"; break;
        default: type = "?";
    }

    switch (message_severity)
    {
#define _TEMP_LOG_(logger) logger("[validation layer][{}]: {}", type, callback_data->pMessage)
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: _TEMP_LOG_(SPDLOG_WARN); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: _TEMP_LOG_(SPDLOG_ERROR); break;
        default: _TEMP_LOG_(SPDLOG_INFO);
#undef _TEMP_LOG_
    }

    return VK_FALSE;
}


void Application::mainLoop()
{
    while (!glfwWindowShouldClose(_window))
    {
        glfwPollEvents();
        if (glfwGetKey(_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(_window, true);
        draw_frame();
    }

    // 等待 device 上的所有 queue 中的操作都完成
    vkDeviceWaitIdle(_device);
}


void Application::record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_idx)
{
    vk::CommandBuffer cmd(command_buffer);

    VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags            = 0,
            .pInheritanceInfo = nullptr,
    };
    if (vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("failed to begin command buffer.");

    VkClearValue clear_color = {.color = {.float32{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderPassBeginInfo render_pass_begin_info = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass      = _render_pass,
            .framebuffer     = _swapchain_framebuffer_list[image_idx],
            .renderArea      = {.offset = {0, 0}, .extent = _swapchain_extent},
            .clearValueCount = 1,
            .pClearValues    = &clear_color,
    };

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);

    // params: firstBinding, buffers, offsets
    cmd.bindVertexBuffers(0, {_vertex_buffer_}, {0});

    // params: vertexCount, instanceCount, firstVertex, firstInstance
    vkCmdDraw(command_buffer, (uint32_t) vertices.size(), 1, 0, 0);
    vkCmdEndRenderPass(command_buffer);
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("failed to end command buffer.");
}


void Application::create_command_pool()
{
    assert(_physical_device != VK_NULL_HANDLE);
    VkCommandPoolCreateInfo pool_create_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = _physical_device_info.graphics_queue_family_idx.value(),
    };
    if (vkCreateCommandPool(_device, &pool_create_info, nullptr, &_command_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool.");
}


void Application::cleanup()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FILGHT; ++i)
    {
        vkDestroySemaphore(_device, _frames[i].render_finish_semaphore, nullptr);
        vkDestroySemaphore(_device, _frames[i].image_available_semaphore, nullptr);
        vkDestroyFence(_device, _frames[i].in_flight_fence, nullptr);
    }

    // command buffer 会跟随 command pool 一起释放
    vkDestroyCommandPool(_device, _command_pool, nullptr);

    clean_swapchain();

    _device_.destroyBuffer(_vertex_buffer_);
    _device_.freeMemory(_vertex_buffer_memory_);
    _device_.destroyBuffer(_index_buffer);
    _device_.freeMemory(_index_buffer_memory);

    _device_.destroy();
    vkDestroySurfaceKHR(_instance, _surface, nullptr);

    // vk debug messenger，需要在 instance 之前销毁
    vk::Instance instance = _instance;
    instance.destroyDebugUtilsMessengerEXT(_debug_messenger);

    // vk instance
    vkDestroyInstance(_instance, nullptr);

    // glfw
    glfwDestroyWindow(_window);
    glfwTerminate();
}


void Application::pick_physical_device()
{
    // 这个函数需要在 instance 创建之后执行
    assert(_instance != VK_NULL_HANDLE);

    // 获得 physical device 的列表
    uint32_t device_cnt = 0;
    vkEnumeratePhysicalDevices(_instance, &device_cnt, nullptr);
    if (device_cnt == 0)
        throw std::runtime_error("no physical device found with vulkan support.");
    std::vector<VkPhysicalDevice> physical_device_list(device_cnt);
    vkEnumeratePhysicalDevices(_instance, &device_cnt, physical_device_list.data());

    // 获得每个 physical device 的信息，并打印出来
    std::vector<PhysicalDeviceInfo> physical_device_info_list;
    for (const auto &device_: physical_device_list)
    {
        PhysicalDeviceInfo info = get_physical_device_info(device_);
        print_physical_device_info(info);
        physical_device_info_list.push_back(info);
    }

    // 检查 physical device 是否符合要求，只需要其中一块就够了
    for (int i = 0; i < device_cnt; ++i)
    {
        if (is_physical_device_suitable(physical_device_info_list[i]))
        {
            _physical_device      = physical_device_list[i];
            _physical_device_info = physical_device_info_list[i];
            break;
        }
    }

    if (_physical_device == VK_NULL_HANDLE)
        throw std::runtime_error("failed to find a suitable GPU.");
}


bool Application::check_physical_device_ext(const PhysicalDeviceInfo &physical_device_info)
{
    std::set<std::string> required_ext_list(_device_ext_list.begin(), _device_ext_list.end());
    for (const auto &ext: physical_device_info.support_ext_list)
        required_ext_list.erase(ext.extensionName);
    return required_ext_list.empty();
}


bool Application::is_physical_device_suitable(const PhysicalDeviceInfo &physical_device_info)
{
    // 检查 features
    if (!physical_device_info.features.tessellationShader)
        return false;

    // 检查 queue family
    if (!physical_device_info.present_queue_family_idx.has_value() ||
        !physical_device_info.graphics_queue_family_idx.has_value())
        return false;

    // 检查 extension
    if (!check_physical_device_ext(physical_device_info))
        return false;

    // 检查对 swap chain 的支持
    if (physical_device_info.device_surface_format_list.empty() ||
        physical_device_info.device_surface_present_mode.empty())
        return false;

    return true;
}


void Application::create_render_pass()
{
    assert(_device != VK_NULL_HANDLE);

    VkAttachmentDescription color_attachment = {
            .format  = _swapchain_iamge_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,

            // 发生在第一次访问 color/depth attchment 之前
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,

            // 发生在最后一次访问 color/depth attachment 之后
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,

            // 在 subpass 前后要对 stencil 做什么，这里是 color attachment，所以不需要
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

            /// 在 renderpass 开始和结束的时候应该对图像使用什么 layout
            /// 由于不需要从 attachment 中读取数据，因此并不关心其 layout，驱动可以做一些优化
            /// subpass 之后，attachment 需要交给 present engine 显示，因此设为 engine 需要的 layout
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,    // 用于 swapchain
    };

    // subpass 引用 color attachment
    VkAttachmentReference color_attachment_ref = {
            .attachment = 0,    // index in render pass

            // 当前 subpass 需要 attachment 是什么样的 layout，由于之前的 attachment description 中
            // 已经说明了 initial layout 是 undefined 的，因此驱动很容易优化
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            // layout(location = 0) out vec4 color;
            .colorAttachmentCount = 1,
            .pColorAttachments    = &color_attachment_ref,
    };

    /// dst subpass 依赖于 src subpass
    /// 非常好的解答：https://www.reddit.com/r/vulkan/comments/s80reu/subpass_dependencies_what_are_those_and_why_do_i/
    VkSubpassDependency dependency = {
            /// VK_SUBPASS_EXTERNAL 表示 dst subpass 依赖于 之前的 renderpass 中的所有 subpass
            .srcSubpass    = VK_SUBPASS_EXTERNAL,
            .dstSubpass    = 0,
            .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_info = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments    = &color_attachment,
            .subpassCount    = 1,
            .pSubpasses      = &subpass,
            .dependencyCount = 1,
            .pDependencies   = &dependency,
    };

    if (vkCreateRenderPass(_device, &render_pass_info, nullptr, &_render_pass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass!");
}


void Application::create_logical_device()
{
    assert(_physical_device != VK_NULL_HANDLE);    // 创建 logical device 需要 physical device

    // 创建 queue 需要的信息
    std::vector<VkDeviceQueueCreateInfo> queue_create_info_list;
    float queue_priority = 1.f;    // 每个队列的优先级，1.f 最高，0.f 最低
    // 可能同一个 queue family 支持多种特性，因此用 set 去重
    for (uint32_t queue_family: std::set<uint32_t>{_physical_device_info.graphics_queue_family_idx.value(),
                                                   _physical_device_info.present_queue_family_idx.value()})
        queue_create_info_list.push_back({
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = queue_family,
                .queueCount       = 1,
                .pQueuePriorities = &queue_priority,
        });

    // locgical device 需要的 feature
    VkPhysicalDeviceFeatures device_feature{};

    VkDeviceCreateInfo device_create_info = {
            .sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = (uint32_t) queue_create_info_list.size(),
            .pQueueCreateInfos    = queue_create_info_list.data(),

            /// logical device 也需要有 validation layer
            /// 但是最新的实现已经不需要专门为 logical device 指定 validation layer 了
            .enabledLayerCount   = (uint32_t) _instance_layer_list.size(),
            .ppEnabledLayerNames = _instance_layer_list.data(),

            .enabledExtensionCount   = (uint32_t) _device_ext_list.size(),
            .ppEnabledExtensionNames = _device_ext_list.data(),

            .pEnabledFeatures = &device_feature,    // 指定 logical device 的 features
    };

    if (vkCreateDevice(_physical_device, &device_create_info, nullptr, &_device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device.");
    _device_ = vk::Device(_device);

    /// 取得 LOGICAL DEVICE 的各个 QUEUE 的 HANDLE
    /// 第二个参数表示 queue family
    /// 第三个参数表示该 queue family 中 queue 的 index，
    /// 因为只创建了一个 graphics queue family 的 queue，因此 index = 0
    vkGetDeviceQueue(_device, _physical_device_info.graphics_queue_family_idx.value(), 0, &_graphics_queue);
    _graphics_queue_ = _graphics_queue;

    // 如果同一个 queue family 既支持 graphics，又支持 present，那么 graphics_queue == present_queue
    vkGetDeviceQueue(_device, _physical_device_info.present_queue_family_idx.value(), 0, &_present_queue);
}


void Application::draw_frame()
{
    // UINT64_MAX 表示等待很长时间
    vkWaitForFences(_device, 1, &_frames[_current_frame_idx].in_flight_fence, VK_TRUE, UINT64_MAX);

    uint32_t image_idx;
    /// 向 swapchain 请求一个 presentable 的 image，可能此时 presentation engine 正在读这个 image。
    /// 在 presentation engine 读完 image 后，它会把 semaphore 设为 signaled
    VkResult result =
            vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX,
                                  _frames[_current_frame_idx].image_available_semaphore, VK_NULL_HANDLE, &image_idx);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate_swapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swapchain image.");
    }

    vkResetFences(_device, 1, &_frames[_current_frame_idx].in_flight_fence);

    // 初始化 command buffer，往里填入内容，然后提交给 GPU
    vk::CommandBuffer cur_cmd_buffer{_frames[_current_frame_idx].command_buffer};
    cur_cmd_buffer.reset({});
    record_draw_command_(cur_cmd_buffer, image_idx);
    vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

    vk::Semaphore wait_semaphores[]   = {_frames[_current_frame_idx].image_available_semaphore};
    vk::Semaphore signal_semaphores[] = {_frames[_current_frame_idx].render_finish_semaphore};

    _graphics_queue_.submit(
            {
                    vk::SubmitInfo{
                            // 在写 color attachment 的阶段等待 semaphore，semaphore 表示 image 可用（可以写入）
                            .waitSemaphoreCount = 1,
                            .pWaitSemaphores    = wait_semaphores,
                            .pWaitDstStageMask  = wait_stages,

                            .commandBufferCount = 1,
                            .pCommandBuffers    = &cur_cmd_buffer,

                            // 在 command buffer 执行完成后，会 signal 哪些 semaphores
                            .signalSemaphoreCount = 1,
                            .pSignalSemaphores    = signal_semaphores,
                    },
            },
            vk::Fence(_frames[_current_frame_idx].in_flight_fence));


    VkSwapchainKHR swapchains[]   = {_swapchain};
    VkPresentInfoKHR present_info = {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &_frames[_current_frame_idx].render_finish_semaphore,
            .swapchainCount     = 1,
            .pSwapchains        = swapchains,
            .pImageIndices      = &image_idx,
            .pResults           = nullptr,
    };

    result = vkQueuePresentKHR(_present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _framebuffer_resized)
    {
        _framebuffer_resized = false;
        recreate_swapchain();
    } else if (result != VK_SUCCESS)
        throw std::runtime_error("failed to present swapchain image.");

    _current_frame_idx = (_current_frame_idx + 1) % MAX_FRAMES_IN_FILGHT;
}


PhysicalDeviceInfo Application::get_physical_device_info(VkPhysicalDevice physical_device)
{
    assert(_surface);
    PhysicalDeviceInfo info{};

    // properties and feature
    vkGetPhysicalDeviceProperties(physical_device, &info.properties);
    vkGetPhysicalDeviceFeatures(physical_device, &info.features);

    // queue families
    uint32_t queue_family_cnt = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_cnt, nullptr);
    info.queue_families.resize(queue_family_cnt);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_cnt, info.queue_families.data());
    for (int i = 0; i < queue_family_cnt; ++i)
    {
        if (info.queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            info.graphics_queue_family_idx = i;
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, _surface, &present_support);
        if (present_support)
            info.present_queue_family_idx = i;
    }

    // swapchain
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, _surface, &info.device_surface_capabilities);
    uint32_t format_cnt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, _surface, &format_cnt, nullptr);
    if (format_cnt != 0)
    {
        info.device_surface_format_list.resize(format_cnt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, _surface, &format_cnt,
                                             info.device_surface_format_list.data());
    }
    uint32_t present_mode_cnt;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, _surface, &present_mode_cnt, nullptr);
    if (present_mode_cnt != 0)
    {
        info.device_surface_present_mode.resize(present_mode_cnt);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, _surface, &present_mode_cnt,
                                                  info.device_surface_present_mode.data());
    }

    // extensions
    uint32_t ext_cnt;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_cnt, nullptr);
    info.support_ext_list.resize(ext_cnt);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_cnt, info.support_ext_list.data());

    // memory properties
    vkGetPhysicalDeviceMemoryProperties(physical_device, &info.memory_properties);

    return info;
}


void Application::print_physical_device_info(const PhysicalDeviceInfo &physical_device_info)
{
    std::stringstream ss;

    // properties and features
    ss << "physical deivice info: \n";
    ss << "\t name: " << physical_device_info.properties.deviceName << "\n";
    ss << "\t type: " << physical_device_info.properties.deviceType
       << "(0:other, 1:integrated-gpu, 2:discretegpu, 3:virtual-gpu, 4:cpu)\n";
    ss << "\t geometry shader(bool): " << physical_device_info.features.geometryShader << "\n";
    ss << "\t tessellation shader(bool): " << physical_device_info.features.tessellationShader << "\n";

    // extensions
    ss << "extensions: \n";
    for (const auto &ext: physical_device_info.support_ext_list)
        ss << "\t" << ext.extensionName << "\n";

    SPDLOG_INFO("{}", ss.str());
}


void Application::create_frame_synchro_data()
{
    assert(_device != VK_NULL_HANDLE);
    assert(_command_pool != VK_NULL_HANDLE);


    _frames.resize(MAX_FRAMES_IN_FILGHT);


    // 为每一帧创建 command buffer
    std::vector<VkCommandBuffer> command_buffers;
    command_buffers.resize(MAX_FRAMES_IN_FILGHT);
    VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {
            .sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = _command_pool,
            // primary: can be submitted to a queue for execution
            // secondary: can not be submitted to a queue for execution
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = (uint32_t) MAX_FRAMES_IN_FILGHT,
    };
    if (vkAllocateCommandBuffers(_device, &cmd_buffer_alloc_info, command_buffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffer.");
    for (int i = 0; i < MAX_FRAMES_IN_FILGHT; ++i)
        _frames[i].command_buffer = command_buffers[i];


    // 为每一帧创建同步对象
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finish_semaphores;
    std::vector<VkFence> in_flight_fences;
    VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            // 让 fence 一开始就是 signaled，因为每一帧都会等待 fence
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (size_t i = 0; i < MAX_FRAMES_IN_FILGHT; ++i)
    {
        if (vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_frames[i].image_available_semaphore) !=
                    VK_SUCCESS ||
            vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_frames[i].render_finish_semaphore) !=
                    VK_SUCCESS ||
            vkCreateFence(_device, &fence_create_info, nullptr, &_frames[i].in_flight_fence) != VK_SUCCESS)
            throw std::runtime_error("failed to create image available semaphore!");
    }
}


void Application::recreate_swapchain()
{
    assert(_device != VK_NULL_HANDLE);

    // 如果是最小化操作，就暂停程序
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwWaitEvents();
        glfwGetFramebufferSize(_window, &width, &height);
    }

    // 等待 device 上所有 queue 中的任务等执行完毕，再更换 swapchain
    vkDeviceWaitIdle(_device);

    // 回收之前的和 swapchain 有关的资源
    clean_swapchain();

    create_swap_chain();
    create_image_views();
    create_render_pass();
    create_pipeline();
    create_framebuffers();
}


void Application::clean_swapchain()
{
    for (auto framebuffer: _swapchain_framebuffer_list)
        vkDestroyFramebuffer(_device, framebuffer, nullptr);

    vkDestroyPipeline(_device, _graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);

    vkDestroyRenderPass(_device, _render_pass, nullptr);

    for (auto image_view: _swapchain_image_view_list)
        vkDestroyImageView(_device, image_view, nullptr);

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}


void Application::create_vertex_buffer()
{
    VkBufferCreateInfo create_info = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = sizeof(vertices[0]) * vertices.size(),
            .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,    // 仅在 graphics queue family 访问
    };

    if (vkCreateBuffer(_device, &create_info, nullptr, &_vertex_buffer) != VK_SUCCESS)
        throw std::runtime_error("failed to create vertex buffer.");

    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 可以确保内存可以被立刻写入 GPU 的缓冲区
    _vertex_buffer_memory = alloc_buffer_memory(_vertex_buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkBindBufferMemory(_device, _vertex_buffer, _vertex_buffer_memory, 0);

    // 将顶点数据写入 vertex buffer memory
    void *data;
    vkMapMemory(_device, _vertex_buffer_memory, 0, create_info.size, 0, &data);
    memcpy(data, vertices.data(), (size_t) create_info.size);
    vkUnmapMemory(_device, _vertex_buffer_memory);
}


VkDeviceMemory Application::alloc_buffer_memory(VkBuffer buffer, VkMemoryPropertyFlags properties)
{
    assert(_device != VK_NULL_HANDLE);
    assert(_physical_device != VK_NULL_HANDLE);

    /// 获得 buffer 所需的内存类型
    /// 当 physical memory properties 的第 i 种 type 被 buffer 接受时
    /// memoryTypeBits 的第 i 位才是 1
    VkMemoryRequirements mem_require;
    vkGetBufferMemoryRequirements(_device, buffer, &mem_require);

    // 检查是否有合适的内存类型
    std::optional<uint32_t> memory_type_idx;
    for (uint32_t i = 0; i < _physical_device_info.memory_properties.memoryTypeCount; ++i)
    {
        if (mem_require.memoryTypeBits & (1 << i) &&
            (_physical_device_info.memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            memory_type_idx = i;
            break;
        }
    }
    if (!memory_type_idx.has_value())
        throw std::runtime_error("no proper memory type for buffer, failed to allocate buffer.");

    // 分配内存
    VkMemoryAllocateInfo info = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = mem_require.size,
            .memoryTypeIndex = memory_type_idx.value(),
    };

    VkDeviceMemory memory;
    if (vkAllocateMemory(_device, &info, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("fail to allocate vertex buffer.");

    return memory;
}


void Application::create_buffer(vk::DeviceSize size, vk::BufferUsageFlags buffer_usage,
                                vk::MemoryPropertyFlags memory_properties, vk::Buffer &buffer,
                                vk::DeviceMemory &buffer_memory)
{
    buffer = _device_.createBuffer({
            .size        = size,
            .usage       = buffer_usage,
            .sharingMode = vk::SharingMode::eExclusive,
    });

    vk::MemoryRequirements mem_require = _device_.getBufferMemoryRequirements(buffer);

    // 找到合适的 memory type，获得其 index
    std::optional<uint32_t> mem_type_idx;
    for (uint32_t i = 0; i < _physical_device_info.memory_properties.memoryTypeCount; ++i)
    {
        if (!(mem_require.memoryTypeBits & (1 << i)))
            continue;
        if ((vk::MemoryPropertyFlags(_physical_device_info.memory_properties.memoryTypes[i].propertyFlags) &
             memory_properties) != memory_properties)
            continue;
        mem_type_idx = i;
        break;
    }
    if (!mem_type_idx.has_value())
        throw std::runtime_error("no proper memory type for buffer, failed to allocate buffer.");

    buffer_memory = _device_.allocateMemory({
            .allocationSize  = mem_require.size,
            .memoryTypeIndex = mem_type_idx.value(),
    });

    _device_.bindBufferMemory(buffer, buffer_memory, 0);
}


void Application::create_vertex_buffer_()
{
    vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

    // create stage buffer and memory
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    create_buffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stage_buffer,
                  stage_buffer_memory);

    /// copy memory to stage memory
    /// params: device_memory_obj, offset, size, flags
    auto data = _device_.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, vertices.data(), (size_t) buffer_size);
    _device_.unmapMemory(stage_buffer_memory);

    /// create vertex buffer and memory
    /// then transfer stage memory to vertex buffer memory
    create_buffer(buffer_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, _vertex_buffer_, _vertex_buffer_memory_);
    copy_buffer(stage_buffer, _vertex_buffer_, buffer_size);

    // 销毁 stage buffer 以及 memory
    _device_.destroyBuffer(stage_buffer);
    _device_.free(stage_buffer_memory);
}


void Application::copy_buffer(vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size)
{
    assert(_command_pool != VK_NULL_HANDLE);

    vk::CommandBuffer temp_cmd = _device_.allocateCommandBuffers({
            .commandPool        = vk::CommandPool(_command_pool),
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    })[0];

    temp_cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    temp_cmd.copyBuffer(src_buffer, dst_buffer, {vk::BufferCopy{.size = size}});
    temp_cmd.end();

    vk::Queue transfer_queue(_graphics_queue);
    transfer_queue.submit({vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &temp_cmd}});
    transfer_queue.waitIdle();

    _device_.freeCommandBuffers(vk::CommandPool(_command_pool), {temp_cmd});
}


void Application::record_draw_command_(const vk::CommandBuffer &cmd_buffer, uint32_t image_idx)
{
    vk::ClearValue clear_color{.color = {.float32 = std::array<float, 4>{0.f, 0.f, 0.f, 1.f}}};

    cmd_buffer.begin({.flags = {}, .pInheritanceInfo = {}});
    cmd_buffer.beginRenderPass(
            vk::RenderPassBeginInfo{
                    .renderPass      = vk::RenderPass(_render_pass),
                    .framebuffer     = vk::Framebuffer(_swapchain_framebuffer_list[image_idx]),
                    .renderArea      = {.offset = {0, 0},
                                        .extent = {.width = _swapchain_extent.width, .height = _swapchain_extent.height}},
                    .clearValueCount = 1,
                    .pClearValues    = &clear_color,
            },
            vk::SubpassContents::eInline);
    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk::Pipeline(_graphics_pipeline));
    // params: firstBinding, buffers, offsets
    cmd_buffer.bindVertexBuffers(0, {_vertex_buffer_}, {0});
    // params: buffer, offset, buffer_type
    cmd_buffer.bindIndexBuffer(_index_buffer, 0, vk::IndexType::eUint16);
    // params: index count, instance count, first index, vertex offset, first instance
    cmd_buffer.drawIndexed((uint32_t)indices.size(), 1, 0, 0, 0);
    cmd_buffer.endRenderPass();
    cmd_buffer.end();
}


void Application::create_index_buffer()
{
    vk::DeviceSize buffer_size = sizeof(indices[0]) * indices.size();

    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    create_buffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stage_buffer,
                  stage_buffer_memory);

    void *data = _device_.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, indices.data(), (size_t) buffer_size);
    _device_.unmapMemory(stage_buffer_memory);

    create_buffer(buffer_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, _index_buffer, _index_buffer_memory);

    copy_buffer(stage_buffer, _index_buffer, buffer_size);

    _device_.destroyBuffer(stage_buffer);
    _device_.freeMemory(stage_buffer_memory);
}