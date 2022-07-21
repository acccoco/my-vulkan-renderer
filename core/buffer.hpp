#pragma once

#include <filesystem>

#include "./include_vk.hpp"
#include "./device.hpp"
#include "./render_pass.hpp"


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
    OneTimeCmdBuffer(const vk::Device &device, const vk::Queue &queue,
                     const vk::CommandPool &cmd_pool)
        : _device(device),
          _cmd_pool(cmd_pool),
          _queue(queue)
    {
        _cmd_buffer = _device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
                .commandPool        = _cmd_pool,
                .level              = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
        })[0];

        _cmd_buffer.begin(vk::CommandBufferBeginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        });
    }


    void end()
    {
        _cmd_buffer.end();

        _queue.submit({vk::SubmitInfo{
                              .commandBufferCount = 1,
                              .pCommandBuffers    = &_cmd_buffer,
                      }},
                      nullptr);
        _queue.waitIdle();

        _device.freeCommandBuffers(_cmd_pool, {_cmd_buffer});
    }


    vk::CommandBuffer &operator()() { return _cmd_buffer; }


private:
    vk::CommandBuffer _cmd_buffer;
    const vk::Device _device;
    const vk::CommandPool _cmd_pool;
    const vk::Queue _queue;
};


/**
 * 创建一个空的 buffer
 */
void create_buffer(const vk::Device &device, const DeviceInfo &device_info, vk::DeviceSize size,
                   vk::BufferUsageFlags buffer_usage, vk::MemoryPropertyFlags memory_properties,
                   vk::Buffer &buffer, vk::DeviceMemory &buffer_memory);


void create_uniform_buffer(const vk::Device &device, const DeviceInfo &device_info,
                           vk::Buffer &uniform_buffer, vk::DeviceMemory &uniform_memory);


vk::DescriptorPool create_descriptor_pool(const vk::Device &device, uint32_t frames_in_flight);

vk::CommandPool create_command_pool(const vk::Device &device, const DeviceInfo &device_info);


/**
 * 更新 uniform buffer 的内容，更新 model 矩阵，让物体旋转起来
 */
void update_uniform_memory(const vk::Device &device, const SurfaceInfo &surface_info,
                           const vk::DeviceMemory &uniform_memory);


std::vector<vk::CommandBuffer> create_command_buffer(const vk::Device &device,
                                                     const vk::CommandPool &command_pool,
                                                     uint32_t frames_in_flight);


// TODO 将多个临时 command buffer 合起来，使用 setup() 和 flush() 来控制
