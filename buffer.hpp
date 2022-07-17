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


/**
 * 创建 index buffer，并且把 indices 数据填入其中
 */
void create_index_buffer(const vk::Device &device, const DeviceInfo &device_info,
                         const vk::CommandPool &cmd_pool, const vk::Queue &transfer_queue,
                         const std::vector<uint16_t> &indices, vk::Buffer &index_buffer,
                         vk::DeviceMemory &index_memory);


/**
 * 创建 vertex buffer，将 vertex 数据填入其中
 */
void create_vertex_buffer_(const vk::Device &device, const DeviceInfo &device_info,
                           const vk::CommandPool &cmd_pool, const vk::Queue &transfer_queue,
                           const std::vector<Vertex> &vertices, vk::Buffer &vertex_buffer,
                           vk::DeviceMemory &vertex_memory);


void create_uniform_buffer(const vk::Device &device, const DeviceInfo &device_info,
                           vk::Buffer &uniform_buffer, vk::DeviceMemory &uniform_memory);


std::vector<vk::Framebuffer>
create_framebuffers(const vk::Device &device, const SurfaceInfo &surface_info,
                    const std::vector<vk::ImageView> &swapchain_image_view_list,
                    const vk::RenderPass &render_pass);


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


/**
 * 创建 image 和对应的 memory，并将两者绑定
 */
void create_image(const vk::Device &device, const DeviceInfo &device_info,
                  const vk::ImageCreateInfo &image_info, const vk::ImageUsageFlags &image_usage,
                  const vk::MemoryPropertyFlags &mem_properties, vk::Image &image,
                  vk::DeviceMemory &memory);


// TODO 将多个临时 command buffer 合起来，使用 setup() 和 flush() 来控制
/**
 * 从文件中读取 texture 信息，创建 image 和 memory
 */
void create_tex_image(const vk::Device &device, const DeviceInfo &device_info,
                      const vk::Queue &trans_queue, const vk::CommandPool &cmd_pool,
                      const std::string &file_path, vk::Image &tex_image,
                      vk::DeviceMemory &tex_memory);


/**
 * 使用 image memory barrier 来转换 image layout
 */
void trans_image_layout(const vk::Device &device, const vk::Queue &trans_queue,
                        const vk::CommandPool &cmd_pool, vk::Image &image, const vk::Format &format,
                        const vk::ImageLayout &old_layout, const vk::ImageLayout &new_layout);


void buffer_image_copy(const vk::Device &device, const vk::Queue &trans_queue,
                       const vk::CommandPool &cmd_pool, vk::Buffer &buffer, vk::Image &image,
                       uint32_t width, uint32_t height);


vk::ImageView img_view_create(const vk::Device &device, const vk::Image &tex_img,
                              const vk::Format &format);


vk::Sampler sampler_create(const vk::Device &device, const DeviceInfo &device_info);