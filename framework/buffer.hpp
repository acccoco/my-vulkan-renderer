#pragma once

#include <filesystem>

#include "./include_vk.hpp"
#include "./env.hpp"
#include "env.hpp"


// TODO 将多个临时 command buffer 合起来，使用 setup() 和 flush() 来控制
/**
 * 只用一次的 command buffer 用完以后就销毁
 * 使用实例：
 *  auto cmd_buffer = OneTimeCmbBuffer(...);
 *  cmd_buffer().xxx();
 *  cmd_buffer.end();
 */
class OneTimeCmdBuffer
{
public:
    OneTimeCmdBuffer()
    {
        auto env = Hiss::Env::env();

        _commit_queue = env->graphics_cmd_pool.commit_queue.queue;
        _pool         = env->graphics_cmd_pool.pool;
        _cmd_buffer   = env->device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
                  .commandPool        = _pool,
                  .level              = vk::CommandBufferLevel::ePrimary,
                  .commandBufferCount = 1,
        })[0];

        _cmd_buffer.begin(vk::CommandBufferBeginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        });
    }


    void end()
    {
        auto env = Hiss::Env::env();

        _cmd_buffer.end();

        _commit_queue.submit({vk::SubmitInfo{
                                     .commandBufferCount = 1,
                                     .pCommandBuffers    = &_cmd_buffer,
                             }},
                             nullptr);
        _commit_queue.waitIdle();

        env->device.free(_pool, {_cmd_buffer});
    }


    vk::CommandBuffer &operator()() { return _cmd_buffer; }


private:
    vk::CommandBuffer _cmd_buffer;
    vk::Queue _commit_queue;
    vk::CommandPool _pool;
};


/**
 * 创建一个空的 buffer
 */
void buffer_create(vk::DeviceSize size, vk::BufferUsageFlags buffer_usage,
                   vk::MemoryPropertyFlags memory_properties, vk::Buffer &buffer,
                   vk::DeviceMemory &buffer_memory);


template<typename U>
void uniform_buffer_create(vk::Buffer &uniform_buffer, vk::DeviceMemory &uniform_mem)
{
    buffer_create(sizeof(U), vk::BufferUsageFlagBits::eUniformBuffer,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  uniform_buffer, uniform_mem);
}


vk::DescriptorPool create_descriptor_pool(uint32_t frames_in_flight);
