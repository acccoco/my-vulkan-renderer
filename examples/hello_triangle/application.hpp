#pragma once

#include <set>
#include <map>
#include <memory>
#include <limits>
#include <cassert>
#include <sstream>
#include <algorithm>


#include "profile.hpp"
#include <model.hpp>
#include <frames.hpp>
#include <global.hpp>
#include <buffer.hpp>
#include <vertex.hpp>
#include <texture.hpp>
#include <swapchain.hpp>
#include <render_pass.hpp>
#include <framebuffer.hpp>


// 窗口的尺寸，单位不是 pixel
const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;


//
std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,

        4, 5, 6, 6, 7, 4,
};


// 三角形的顶点数据
std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.f, .5f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, .5f}},

        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
};


class Application
{
public:
    void run()
    {
        LogStatic::init();
        WindowStatic::init(WIDTH, HEIGHT);

        init_application();

        // main loop
        while (!glfwWindowShouldClose(WindowStatic::window_get()))
        {
            glfwPollEvents();
            if (glfwGetKey(WindowStatic::window_get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(WindowStatic::window_get(), true);
            draw();
        }


        EnvSingleton::env()->device.waitIdle();


        cleanup();
        WindowStatic::close();
    }


private:
#pragma region members
    static constexpr uint32_t MAX_FRAMES_INFLIGHT = 2;

    vk::Instance _instance;
    std::shared_ptr<Swapchain> _swapchain;
    std::shared_ptr<MSAAFramebuffer> _framebuffer;

    /* 控制 GPU 最多可以同时处理多少 frames */
    std::shared_ptr<FramesInflight<MAX_FRAMES_INFLIGHT>> _inflight;


    vk::RenderPass _render_pass;
    vk::Pipeline _graphics_pipeline;
    vk::PipelineLayout _pipeline_layout;
    vk::DescriptorSetLayout _descriptor_set_layout;


    vk::Buffer _vertex_buffer;
    vk::DeviceMemory _vertex_memory;
    vk::Buffer _index_buffer;
    vk::DeviceMemory _index_memory;
    vk::DescriptorPool _descriptor_pool;
    std::vector<vk::DescriptorSet> _descriptor_sets;


    TestModel model;
    Texture _tex;


    FramebufferLayout_temp _framebuffer_layout;


#pragma endregion


    void init_application()
    {
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        _instance = instance_create(DebugUtils::dbg_msg_info);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);
        DebugUtils::msger_init(_instance);


        /* device 相关 */
        EnvSingleton::init_once(_instance);
        auto env   = EnvSingleton::env();
        _inflight  = FramesInflight<MAX_FRAMES_INFLIGHT>::create();
        _swapchain = Swapchain::create();


        /* framebuffer 的各个 attachment 的格式 */
        auto depth_format = env->format_filter(
                {
                        vk::Format::eD32Sfloat,
                        vk::Format::eD32SfloatS8Uint,
                        vk::Format::eD24UnormS8Uint,
                },
                vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        if (!depth_format.has_value())
            throw std::runtime_error("failed to find supported format.");
        _framebuffer_layout = FramebufferLayout_temp{
                .color_format   = env->present_format.format,
                .color_sample   = env->max_sample_cnt(),
                .depth_format   = depth_format.value(),
                .depth_sample   = env->max_sample_cnt(),
                .resolve_format = _swapchain->format(),
                .resolve_sample = vk::SampleCountFlagBits::e1,
        };


        /* render pass 和 pipeline */
        _descriptor_set_layout = descriptor_set_layout_create();
        _pipeline_layout       = pipeline_layout_create({_descriptor_set_layout});
        _render_pass           = render_pass_create(_framebuffer_layout);
        _graphics_pipeline     = pipeline_create(_pipeline_layout, _render_pass);


        _framebuffer = MSAAFramebuffer::create(_render_pass, _framebuffer_layout,
                                               _swapchain->img_views(), env->present_extent);


        /* 绘制的对象相关 */
        vertex_buffer_create(vertices, _vertex_buffer, _vertex_memory);
        index_buffer_create(indices, _index_buffer, _index_memory);
        _tex = Texture::load(TEXTURE("viking_room.png"), vk::Format::eR8G8B8A8Srgb,
                             vk::ImageAspectFlagBits::eColor);

        _descriptor_pool = create_descriptor_pool(MAX_FRAMES_INFLIGHT);
        _descriptor_sets = create_descriptor_set(_descriptor_set_layout, _descriptor_pool,
                                                 MAX_FRAMES_INFLIGHT, _inflight->uniform_buffers(),
                                                 _tex.img_view(), _tex.sampler());

        model.model_load();
    }


    void cleanup()
    {
        vk::Device temp_device = EnvSingleton::env()->device;


        _inflight = nullptr;


        // 各种 buffer
        temp_device.destroyBuffer(_vertex_buffer);
        temp_device.free(_vertex_memory);
        temp_device.destroyBuffer(_index_buffer);
        temp_device.free(_index_memory);
        _tex.free();
        temp_device.destroyDescriptorPool(_descriptor_pool);

        model.resource_free();


        // render pass
        temp_device.destroyRenderPass(_render_pass);
        temp_device.destroyPipeline(_graphics_pipeline);
        temp_device.destroyPipelineLayout(_pipeline_layout);
        temp_device.destroyDescriptorSetLayout(_descriptor_set_layout);


        // swapchain
        _framebuffer = nullptr;
        _swapchain   = nullptr;


        // device
        EnvSingleton::free(_instance);
        DebugUtils::msger_free(_instance);
        _instance.destroy();
    }


    void draw()
    {
        auto env = EnvSingleton::env();


        /* 等待 fence 进入 signal 状态 */
        (void) env->device.waitForFences({_inflight->current_inflight_fence()}, VK_TRUE,
                                         UINT64_MAX);


        /**
         * 向 swapchain 请求一个 presentable 的 image，可能此时 presentation engine 正在读这个 image。
         * 在 presentation engine 读完 image 后，它会把 semaphore 设为 signaled
         * 如果 swapchain 已经不适合 surface 了，就重新创建一个 swapchain
         */
        Recreate need_recreate;
        uint32_t image_idx;
        std::tie(need_recreate, image_idx) =
                _swapchain->next_img_acquire(_inflight->current_img_available_semaphore());
        if (need_recreate == Recreate::NEED)
        {
            recreate_swapchain();
            return;
        }


        /* 将 fence 设为 unsignal state */
        env->device.resetFences({_inflight->current_inflight_fence()});


        // 更新 MVP 矩阵
        update_uniform_memory(_inflight->current_uniform_mem());


        /* 设置 clear value，顺序应该和 framebuffer 中 attachment 的顺序一致 */
        std::array<vk::ClearValue, 2> clear_values = {
                vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.f, 0.f, 0.f, 1.f}}},
                vk::ClearValue{.depthStencil = {1.f, 0}},
        };


        vk::RenderPassBeginInfo render_pass_info = {
                .renderPass      = _render_pass,
                .framebuffer     = _framebuffer->framebuffer_get(image_idx),
                .renderArea      = {.offset = {0, 0}, .extent = env->present_extent},
                .clearValueCount = static_cast<uint32_t>(clear_values.size()),
                .pClearValues    = clear_values.data(),
        };


        /* record command */
        vk::CommandBuffer cur_cmd_buffer = _inflight->current_cmd_buffer();
        {
            cur_cmd_buffer.reset();
            cur_cmd_buffer.begin(vk::CommandBufferBeginInfo{});

            /* render pass */
            {
                cur_cmd_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
                cur_cmd_buffer.bindVertexBuffers(0, {model.vertex_buffer()}, {0});
                cur_cmd_buffer.bindIndexBuffer(model.index_buffer(), 0, vk::IndexType::eUint32);
                cur_cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _graphics_pipeline);
                cur_cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                  _pipeline_layout, 0,
                                                  {_descriptor_sets[_inflight->current_idx()]}, {});

                /* draw 需要在 bind 之后执行 */
                cur_cmd_buffer.drawIndexed(static_cast<uint32_t>(model.index_cnt()), 1, 0, 0, 0);
                cur_cmd_buffer.endRenderPass();
            }

            cur_cmd_buffer.end();
        }


        // 提交绘制命令
        std::vector<vk::CommandBuffer> commit_cmd_buffers = {cur_cmd_buffer};
        std::vector<vk::Semaphore> wait_semaphores = {_inflight->current_img_available_semaphore()};
        std::vector<vk::PipelineStageFlags> wait_stages = {
                vk::PipelineStageFlagBits::eColorAttachmentOutput};
        std::vector<vk::Semaphore> signal_semaphores = {
                _inflight->current_render_finish_semaphore()};
        env->graphics_cmd_pool.commit_queue().submit(
                {vk::SubmitInfo{
                        .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size()),
                        .pWaitSemaphores    = wait_semaphores.data(),
                        .pWaitDstStageMask  = wait_stages.data(),

                        .commandBufferCount = static_cast<uint32_t>(commit_cmd_buffers.size()),
                        .pCommandBuffers    = commit_cmd_buffers.data(),

                        .signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size()),
                        .pSignalSemaphores    = signal_semaphores.data(),
                }},
                _inflight->current_inflight_fence());


        // 将结果送到 surface 显示
        need_recreate =
                _swapchain->present(image_idx, {_inflight->current_render_finish_semaphore()});
        if (need_recreate == Recreate::NEED)
            recreate_swapchain();


        // 最后交换 in flight frame index
        _inflight->next_frame();
    }


    /**
     * window 大小改变，重新创建一系列资源
     */
    void recreate_swapchain()
    {
        LogStatic::logger()->info("[window] window resized, recreate resource.");
        auto env = EnvSingleton::env();


        /* 如果窗口最小化，先暂停程序 */
        WindowStatic::wait_exit_minimize();


        /* 等待 device 上的任务执行完毕 */
        env->device.waitIdle();


        /* 回收旧的资源 */
        _framebuffer = nullptr;
        env->device.destroyPipeline(_graphics_pipeline);
        _swapchain = nullptr;


        /* 创建新的资源 */
        EnvSingleton::surface_recreate(_instance);
        _swapchain         = Swapchain::create();
        _graphics_pipeline = pipeline_create(_pipeline_layout, _render_pass);
        _framebuffer =
                MSAAFramebuffer::create(_render_pass, _framebuffer_layout, _swapchain->img_views(),
                                        EnvSingleton::env()->present_extent);
    }


    /**
     * 更新 uniform buffer 的内容，更新 model 矩阵，让物体旋转起来
     */
    static void update_uniform_memory(const vk::DeviceMemory &uniform_memory)
    {
        auto env = *EnvSingleton::env();

        static auto start_time = std::chrono::high_resolution_clock::now();

        auto cur_time = std::chrono::high_resolution_clock::now();
        float time =
                std::chrono::duration<float, std::chrono::seconds::period>(cur_time - start_time)
                        .count();

        UniformBufferObject ubo = {
                .model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f),
                                     glm::vec3(0.f, 1.f, 0.f)),

                .view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f),
                                    glm::vec3(0.f, 1.f, 0.f)),

                /* 可以确保 surface extent 是最新的值，因此拉伸窗口物体不会变形 */
                .proj = glm::perspective(glm::radians(45.f),
                                         (float) env.present_extent.width
                                                 / (float) env.present_extent.height,
                                         0.1f, 10.f),
        };
        ubo.proj[1][1] *= -1.f;    // OpenGL 和 vulkan 的坐标系差异

        void *data = env.device.mapMemory(uniform_memory, 0, sizeof(ubo), {});
        std::memcpy(data, &ubo, sizeof(ubo));
        env.device.unmapMemory(uniform_memory);
    }
};