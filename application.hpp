#pragma once

#include <set>
#include <map>
#include <memory>
#include <limits>
#include <cassert>
#include <sstream>
#include <algorithm>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "./buffer.hpp"
#include "./swapchain.hpp"
#include "./render_pass.hpp"


// 用于查找函数地址的 dispatcher，vulkan hpp 提供了一个默认的
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;


// 窗口的尺寸，单位不是 pixel
const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;


class Application
{
public:
    void run()
    {
        init_spdlog();
        _window = init_window(WIDTH, HEIGHT, glfw_user_data);

        init_application();

        // main loop
        while (!glfwWindowShouldClose(_window))
        {
            glfwPollEvents();
            if (glfwGetKey(_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(_window, true);
            draw();
        }


        _device.waitIdle();


        cleanup();
        glfwDestroyWindow(_window);
        glfwTerminate();
    }


private:
#pragma region debug_logger

    inline static std::shared_ptr<spdlog::logger> logger;
    inline static std::shared_ptr<spdlog::logger> val_logger;

    static void init_spdlog()
    {
        logger = spdlog::stdout_color_st("logger");
        logger->set_level(spdlog::level::trace);
        logger->set_pattern("[%^%L%$] %v");


        val_logger = spdlog::stdout_color_st("validation");
        val_logger->set_level(spdlog::level::trace);
        val_logger->set_pattern("[%^%n%$] %v");
    }

    const vk::DebugUtilsMessengerCreateInfoEXT _dbg_msger_info = {
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debug_callback,
            .pUserData       = nullptr,
    };

    static vk::Bool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                     VkDebugUtilsMessageTypeFlagsEXT message_type,
                                     const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *)
    {
        const char *type;
        switch (static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(message_type))
        {
            case vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral: type = "General"; break;
            case vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation: type = "Validation"; break;
            case vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance: type = "Performance"; break;
            default: type = "?";
        }

        switch (message_severity)
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                val_logger->warn("[{}]: {}", type, callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                val_logger->error("[{}]: {}", type, callback_data->pMessage);
                break;
            default: val_logger->info("[{}]: {}", type, callback_data->pMessage);
        }

        return VK_FALSE;
    }
#pragma endregion


#pragma region members
    static const int MAX_FRAMES_IN_FILGHT = 2;    // 最多允许同时处理多少帧
    uint32_t _current_frame_idx           = 0;


    vk::Instance _instance;
    vk::DebugUtilsMessengerEXT _dbg_msger;
    vk::PhysicalDevice _physical_device;
    vk::Device _device;
    DeviceInfo _device_info;
    vk::SurfaceKHR _surface;
    SurfaceInfo _surface_info;
    vk::Queue _graphics_queue;
    vk::Queue _present_queue;
    vk::CommandPool _command_pool;
    std::vector<vk::CommandBuffer> _cmd_buffers;


    GLFWwindow *_window;
    GLFWUserData glfw_user_data;


    vk::SwapchainKHR _swapchain;
    std::vector<vk::Framebuffer> _swapchain_framebuffer_list;
    std::vector<vk::Image> _swapchain_image_list;    // 跟随 swapchain 销毁
    std::vector<vk::ImageView> _swapchain_image_view_list;


    std::vector<vk::Semaphore> _image_available_semaphores;
    std::vector<vk::Semaphore> _render_finish_semaphores;
    std::vector<vk::Fence> _in_flight_fences;


    vk::RenderPass _render_pass;
    vk::Pipeline _graphics_pipeline;
    vk::PipelineLayout _pipeline_layout;
    vk::DescriptorSetLayout _descriptor_set_layout;


    vk::Buffer _vertex_buffer;
    vk::DeviceMemory _vertex_memory;
    vk::Buffer _index_buffer;
    vk::DeviceMemory _index_memory;
    std::vector<vk::Buffer> _uniform_buffers;
    std::vector<vk::DeviceMemory> _uniform_memorys;
    vk::DescriptorPool _descriptor_pool;
    std::vector<vk::DescriptorSet> _descriptor_sets;


    vk::ClearValue _clear_value = {.color = {.float32 = std::array<float, 4>{0.f, 0.f, 0.f, 1.f}}};
#pragma endregion


    void init_application()
    {
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        _instance = create_instance(_dbg_msger_info);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);


        _dbg_msger = set_dbg_msger(_instance, _dbg_msger_info);


        _surface         = create_surface(_instance, _window);
        _physical_device = pick_physical_device(_instance, _surface, _window);
        _device_info     = DeviceInfo::get_info(_physical_device, _surface);
        _surface_info    = SurfaceInfo::get_info(_physical_device, _surface, _window);
        _device          = create_device(_physical_device, _surface, _present_queue, _graphics_queue);
        _command_pool    = create_command_pool(_device, _device_info);
        _cmd_buffers     = create_command_buffer(_device, _command_pool, MAX_FRAMES_IN_FILGHT);
        create_sychronization();


        _swapchain            = create_swapchain(_device, _surface, _device_info, _surface_info);
        _swapchain_image_list = _device.getSwapchainImagesKHR(_swapchain);
        logger->info("swapchain image count: {}", _swapchain_image_list.size());
        _swapchain_image_view_list = create_swapchain_view(_device, _surface_info, _swapchain_image_list);


        _descriptor_set_layout = create_descriptor_set_layout(_device);
        _pipeline_layout       = create_pipelien_layout(_device, {_descriptor_set_layout});
        _render_pass           = create_render_pass(_device, _surface_info);
        _graphics_pipeline     = create_pipeline(_device, _surface_info, _pipeline_layout, _render_pass);


        _swapchain_framebuffer_list =
                create_framebuffers(_device, _surface_info, _swapchain_image_view_list, _render_pass);
        create_vertex_buffer_(_device, _device_info, _command_pool, _graphics_queue, vertices, _vertex_buffer,
                              _vertex_memory);
        create_index_buffer(_device, _device_info, _command_pool, _graphics_queue, indices, _index_buffer,
                            _index_memory);
        _uniform_buffers.resize(MAX_FRAMES_IN_FILGHT);
        _uniform_memorys.resize(MAX_FRAMES_IN_FILGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FILGHT; ++i)
            create_uniform_buffer(_device, _device_info, _uniform_buffers[i], _uniform_memorys[i]);
        _descriptor_pool = create_descriptor_pool(_device, MAX_FRAMES_IN_FILGHT);
        _descriptor_sets = create_descriptor_set(_device, _descriptor_set_layout, _descriptor_pool,
                                                 MAX_FRAMES_IN_FILGHT, _uniform_buffers);
    }


    void cleanup()
    {
        for (size_t i = 0; i < MAX_FRAMES_IN_FILGHT; ++i)
        {
            _device.destroySemaphore(_image_available_semaphores[i]);
            _device.destroySemaphore(_render_finish_semaphores[i]);
            _device.destroyFence(_in_flight_fences[i]);
        }


        _device.destroyBuffer(_vertex_buffer);
        _device.free(_vertex_memory);
        _device.destroyBuffer(_index_buffer);
        _device.free(_index_memory);
        for (size_t i = 0; i < MAX_FRAMES_IN_FILGHT; ++i)
        {
            _device.destroyBuffer(_uniform_buffers[i]);
            _device.free(_uniform_memorys[i]);
        }
        _device.destroyDescriptorPool(_descriptor_pool);


        _device.destroyRenderPass(_render_pass);
        _device.destroyPipeline(_graphics_pipeline);
        _device.destroyPipelineLayout(_pipeline_layout);
        _device.destroyDescriptorSetLayout(_descriptor_set_layout);


        _device.destroySwapchainKHR(_swapchain);
        for (auto &framebuffer: _swapchain_framebuffer_list)
            _device.destroyFramebuffer(framebuffer);
        for (auto &image_view: _swapchain_image_view_list)
            _device.destroyImageView(image_view);


        _device.destroyCommandPool(_command_pool);


        _device.destroy();
        _instance.destroySurfaceKHR(_surface);
        _instance.destroyDebugUtilsMessengerEXT(_dbg_msger);
        _instance.destroy();
    }


    void draw()
    {
        _device.waitForFences({_in_flight_fences[_current_frame_idx]}, VK_TRUE, UINT64_MAX);


        /// 向 swapchain 请求一个 presentable 的 image，可能此时 presentation engine 正在读这个 image。
        /// 在 presentation engine 读完 image 后，它会把 semaphore 设为 signaled
        /// 如果 swapchain 已经不适合 surface 了，就重新创建一个 swapchain
        const auto &[acquire_result, image_idx] = acquireNextImageKHR(
                _device, _swapchain, UINT64_MAX, _image_available_semaphores[_current_frame_idx], {});
        if (acquire_result == vk::Result::eErrorOutOfDateKHR)
        {
            recreate_swapchain();
            return;
        } else if (acquire_result != vk::Result::eSuccess && acquire_result != vk::Result::eSuboptimalKHR)
            throw std::runtime_error("failed to acquire swapchain image.");


        // 确保 swapchain 和 surface 匹配后再 reset fence
        _device.resetFences({_in_flight_fences[_current_frame_idx]});


        // 更新 MVP 矩阵
        update_uniform_memory(_device, _surface_info, _uniform_memorys[_current_frame_idx]);


        // 记录绘制的命令
        vk::CommandBuffer cur_cmd_buffer = _cmd_buffers[_current_frame_idx];
        cur_cmd_buffer.reset();
        cur_cmd_buffer.begin(vk::CommandBufferBeginInfo{});
        cur_cmd_buffer.beginRenderPass(
                vk::RenderPassBeginInfo{
                        .renderPass      = _render_pass,
                        .framebuffer     = _swapchain_framebuffer_list[image_idx],
                        .renderArea      = {.offset = {0, 0}, .extent = _surface_info.extent},
                        .clearValueCount = 1,
                        .pClearValues    = &_clear_value,
                },
                vk::SubpassContents::eInline);
        cur_cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _graphics_pipeline);
        cur_cmd_buffer.bindVertexBuffers(0, {_vertex_buffer}, {0});
        cur_cmd_buffer.bindIndexBuffer(_index_buffer, 0, vk::IndexType::eUint16);
        cur_cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout, 0,
                                          {_descriptor_sets[_current_frame_idx]}, {});
        cur_cmd_buffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        cur_cmd_buffer.endRenderPass();
        cur_cmd_buffer.end();


        // 提交绘制命令
        std::vector<vk::CommandBuffer> commit_cmd_buffers = {cur_cmd_buffer};
        std::vector<vk::Semaphore> wait_semaphores        = {_image_available_semaphores[_current_frame_idx]};
        std::vector<vk::PipelineStageFlags> wait_stages   = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        std::vector<vk::Semaphore> singal_semaphores      = {_render_finish_semaphores[_current_frame_idx]};
        _graphics_queue.submit({vk::SubmitInfo{
                                       .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size()),
                                       .pWaitSemaphores    = wait_semaphores.data(),
                                       .pWaitDstStageMask  = wait_stages.data(),

                                       .commandBufferCount = static_cast<uint32_t>(commit_cmd_buffers.size()),
                                       .pCommandBuffers    = commit_cmd_buffers.data(),

                                       .signalSemaphoreCount = static_cast<uint32_t>(singal_semaphores.size()),
                                       .pSignalSemaphores    = singal_semaphores.data(),
                               }},
                               _in_flight_fences[_current_frame_idx]);


        // 将结果送到 surface 显示
        std::vector<vk::SwapchainKHR> swapchains = {_swapchain};
        std::vector<uint32_t> image_indices      = {image_idx};
        vk::PresentInfoKHR present_info          = {
                         .waitSemaphoreCount = 1,
                         .pWaitSemaphores    = &_render_finish_semaphores[_current_frame_idx],

                         .swapchainCount = static_cast<uint32_t>(swapchains.size()),
                         .pSwapchains    = swapchains.data(),
                         .pImageIndices  = image_indices.data(),
        };
        vk::Result present_result = _present_queue.presentKHR(present_info);
        if (present_result == vk::Result::eErrorOutOfDateKHR || present_result == vk::Result::eSuboptimalKHR ||
            glfw_user_data.framebuffer_resized)
        {
            glfw_user_data.framebuffer_resized = false;
            recreate_swapchain();
        } else if (present_result != vk::Result::eSuccess)
            throw std::runtime_error("failt to present swapchain image.");


        // 最后交换 in flight frame index
        _current_frame_idx = (_current_frame_idx + 1) % MAX_FRAMES_IN_FILGHT;
    }


    void recreate_swapchain()
    {
        /// 如果是窗口最小化操作，就暂停程序
        int width = 0, height = 0;
        glfwGetFramebufferSize(_window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwWaitEvents();
            glfwGetFramebufferSize(_window, &width, &height);
        }


        /// 等待 device 上的所有任务都执行完毕
        _device.waitIdle();


        /// 回收和 surface 相关的资源
        for (auto framebuffer: _swapchain_framebuffer_list)
            _device.destroyFramebuffer(framebuffer);
        _device.destroyPipeline(_graphics_pipeline);
        _device.destroyRenderPass(_render_pass);
        for (auto image_view: _swapchain_image_view_list)
            _device.destroyImageView(image_view);
        _device.destroySwapchainKHR(_swapchain);


        /// 重新创建 swapchain
        _surface_info              = SurfaceInfo::get_info(_physical_device, _surface, _window);
        _swapchain                 = create_swapchain(_device, _surface, _device_info, _surface_info);
        _swapchain_image_list      = _device.getSwapchainImagesKHR(_swapchain);
        _swapchain_image_view_list = create_swapchain_view(_device, _surface_info, _swapchain_image_list);
        _render_pass               = create_render_pass(_device, _surface_info);
        _graphics_pipeline         = create_pipeline(_device, _surface_info, _pipeline_layout, _render_pass);
        _swapchain_framebuffer_list =
                create_framebuffers(_device, _surface_info, _swapchain_image_view_list, _render_pass);
    }


    void create_sychronization()
    {
        spdlog::get("logger")->info("create synchronization.");
        _image_available_semaphores.resize(MAX_FRAMES_IN_FILGHT);
        _render_finish_semaphores.resize(MAX_FRAMES_IN_FILGHT);
        _in_flight_fences.resize(MAX_FRAMES_IN_FILGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FILGHT; ++i)
        {
            _image_available_semaphores[i] = _device.createSemaphore({});
            _render_finish_semaphores[i]   = _device.createSemaphore({});
            _in_flight_fences[i]           = _device.createFence({
                              // 让 fence 初始状态是 signal 的
                              .flags = vk::FenceCreateFlagBits::eSignaled,
            });
        }
    }
};