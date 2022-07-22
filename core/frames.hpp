#pragma once

#include <array>
#include "include_vk.hpp"
#include "env.hpp"
#include "buffer.hpp"
#include "render_pass.hpp"


/**
 * inflight 表示：同一时刻，最多有多少 frame 在 GPU 上渲染
 * 注意和 swapchain 的多重缓冲区分开来
 */
template<uint32_t N>
class FramesInflight
{
private:
    std::array<vk::Semaphore, N> _img_available; /* 从 swapchain 的 image 是否可用 */
    std::array<vk::Semaphore, N> _render_finish; /* render 过程是否完成 */

    std::array<vk::Fence, N> _inflight; /* 是否可以开始准备绘制 */

    std::array<vk::CommandBuffer, N> _cmd_buffers;

    std::array<vk::Buffer, N> _uniform_buffers;
    std::array<vk::DeviceMemory, N> _uniform_mem;

    uint32_t _current_frame_idx = 0;


    FramesInflight()
    {
        auto env = EnvSingleton::env();

        for (size_t i = 0; i < N; ++i)
        {
            _img_available[i] = env->device.createSemaphore({});
            _render_finish[i] = env->device.createSemaphore({});

            _inflight[i] = env->device.createFence({
                    .flags = vk::FenceCreateFlagBits::eSignaled,
            });

            _cmd_buffers[i] = env->device.allocateCommandBuffers({
                    .commandPool        = env->graphics_cmd_pool.pool,
                    .level              = vk::CommandBufferLevel::ePrimary,
                    .commandBufferCount = 1,
            })[0];

            uniform_buffer_create<UniformBufferObject>(_uniform_buffers[i], _uniform_mem[i]);
        }
    }


public:
    static std::shared_ptr<FramesInflight<N>> create()
    {
        return std::shared_ptr<FramesInflight<N>>(new FramesInflight<N>());
    }

    void next_frame() { _current_frame_idx = (_current_frame_idx + 1) % N; }

    vk::Semaphore current_img_available_semaphore() { return _img_available[_current_frame_idx]; }

    vk::Semaphore current_render_finish_semaphore() { return _render_finish[_current_frame_idx]; }

    vk::Fence current_inflight_fence() { return _inflight[_current_frame_idx]; }

    vk::CommandBuffer current_cmd_buffer() { return _cmd_buffers[_current_frame_idx]; }

    std::array<vk::Buffer, N> uniform_buffers() { return _uniform_buffers; }

    vk::DeviceMemory current_uniform_mem() { return _uniform_mem[_current_frame_idx]; }

    uint32_t current_idx() { return _current_frame_idx; }

    ~FramesInflight()
    {
        auto env = EnvSingleton::env();

        for (size_t i = 0; i < N; ++i)
        {
            env->device.destroy(_img_available[i]);
            env->device.destroy(_render_finish[i]);
            env->device.destroy(_inflight[i]);
            env->device.destroy(_uniform_buffers[i]);
            env->device.free(_uniform_mem[i]);
        }
        env->device.free(env->graphics_cmd_pool.pool, _cmd_buffers);
    }
};