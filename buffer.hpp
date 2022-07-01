#pragma once

#include "./include_vk.hpp"
#include "./device.hpp"
#include "./vertex.hpp"


// TODO 这个该放在哪里呢？
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};


/**
 * 向 GPU 提交命令，复制 buffer
 * @param cmd_pool 申请临时 command buffer 的 pool
 * @param transfer_queue 用于向 GPU 提交命令的 queue，支持 graphics 和 compute 的 queue 一定支持 transfer
 */
inline void copy_buffer(const vk::Device &device, const vk::CommandPool &cmd_pool, const vk::Queue &transfer_queue,
                        const vk::Buffer &src_buffer, vk::Buffer &dst_buffer, vk::DeviceSize size)
{
    // 申请临时的 command buffer
    vk::CommandBuffer temp_cmd_buffer = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
            .commandPool        = cmd_pool,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    })[0];

    temp_cmd_buffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    temp_cmd_buffer.copyBuffer(src_buffer, dst_buffer, {vk::BufferCopy{.size = size}});
    temp_cmd_buffer.end();

    transfer_queue.submit({vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &temp_cmd_buffer}});
    transfer_queue.waitIdle();

    // 销毁临时的 command buffer
    device.freeCommandBuffers(vk::CommandPool(cmd_pool), {temp_cmd_buffer});
}


inline void create_buffer(const vk::Device &device, const DeviceInfo &device_info, vk::DeviceSize size,
                          vk::BufferUsageFlags buffer_usage, vk::MemoryPropertyFlags memory_properties,
                          vk::Buffer &buffer, vk::DeviceMemory &buffer_memory)
{
    buffer = device.createBuffer({
            .size        = size,
            .usage       = buffer_usage,
            .sharingMode = vk::SharingMode::eExclusive,
    });

    vk::MemoryRequirements mem_require = device.getBufferMemoryRequirements(buffer);

    // 找到合适的 memory type，获得其 index
    std::optional<uint32_t> mem_type_idx;
    for (uint32_t i = 0; i < device_info.mem_properties.memoryTypeCount; ++i)
    {
        if (!(mem_require.memoryTypeBits & (1 << i)))
            continue;
        if ((vk::MemoryPropertyFlags(device_info.mem_properties.memoryTypes[i].propertyFlags) & memory_properties) !=
            memory_properties)
            continue;
        mem_type_idx = i;
        break;
    }
    if (!mem_type_idx.has_value())
        throw std::runtime_error("no proper memory type for buffer, failed to allocate buffer.");

    buffer_memory = device.allocateMemory({
            .allocationSize  = mem_require.size,
            .memoryTypeIndex = mem_type_idx.value(),
    });

    device.bindBufferMemory(buffer, buffer_memory, 0);
}


inline void create_index_buffer(const vk::Device &device, const DeviceInfo &device_info,
                                const vk::CommandPool &cmd_pool, const vk::Queue &transfer_queue,
                                const std::vector<uint16_t> &indices, vk::Buffer &index_buffer,
                                vk::DeviceMemory &index_memory)
{
    spdlog::get("logger")->info("create index buffer.");

    vk::DeviceSize buffer_size = sizeof(indices[0]) * indices.size();

    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    create_buffer(device, device_info, buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stage_buffer,
                  stage_buffer_memory);

    void *data = device.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, indices.data(), (size_t) buffer_size);
    device.unmapMemory(stage_buffer_memory);

    create_buffer(device, device_info, buffer_size,
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, index_buffer, index_memory);

    copy_buffer(device, cmd_pool, transfer_queue, stage_buffer, index_buffer, buffer_size);

    device.destroyBuffer(stage_buffer);
    device.freeMemory(stage_buffer_memory);
}


inline void create_vertex_buffer_(const vk::Device &device, const DeviceInfo &device_info,
                                  const vk::CommandPool &cmd_pool, const vk::Queue &transfer_queue,
                                  const std::vector<Vertex> &vertices, vk::Buffer &vertex_buffer,
                                  vk::DeviceMemory &vertex_memory)
{
    spdlog::get("logger")->info("create vertex buffer.");


    vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

    // create stage buffer and memory
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    create_buffer(device, device_info, buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stage_buffer,
                  stage_buffer_memory);

    /// copy memory to stage memory
    /// params: device_memory_obj, offset, size, flags
    auto data = device.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, vertices.data(), (size_t) buffer_size);
    device.unmapMemory(stage_buffer_memory);

    /// create vertex buffer and memory
    /// then transfer stage memory to vertex buffer memory
    create_buffer(device, device_info, buffer_size,
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, vertex_buffer, vertex_memory);
    copy_buffer(device, cmd_pool, transfer_queue, stage_buffer, vertex_buffer, buffer_size);

    // 销毁 stage buffer 以及 memory
    device.destroyBuffer(stage_buffer);
    device.free(stage_buffer_memory);
}


inline void create_uniform_buffer(const vk::Device &device, const DeviceInfo &device_info, vk::Buffer &uniform_buffer,
                                  vk::DeviceMemory &uniform_memory)
{
    spdlog::get("logger")->info("create uniform buffer.");

    create_buffer(device, device_info, sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer,
                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, uniform_buffer,
                  uniform_memory);
}


inline std::vector<vk::Framebuffer> create_framebuffers(const vk::Device &device, const SurfaceInfo &surface_info,
                                                        const std::vector<vk::ImageView> &swapchain_image_view_list,
                                                        const vk::RenderPass &render_pass)
{
    spdlog::get("logger")->info("create framebuffers.");
    std::vector<vk::Framebuffer> swapchain_framebuffer_list;
    swapchain_framebuffer_list.resize(swapchain_image_view_list.size());
    for (size_t i = 0; i < swapchain_image_view_list.size(); ++i)
    {
        vk::ImageView attachments[] = {
                swapchain_image_view_list[i],
        };
        vk::FramebufferCreateInfo framebuffer_create_info = {
                .renderPass      = render_pass,
                .attachmentCount = 1,
                .pAttachments    = attachments,
                .width           = surface_info.extent.width,
                .height          = surface_info.extent.height,
                .layers          = 1,
        };

        swapchain_framebuffer_list[i] = device.createFramebuffer(framebuffer_create_info);
    }
    return swapchain_framebuffer_list;
}


inline vk::DescriptorPool create_descriptor_pool(const vk::Device &device, uint32_t frames_in_flight)
{
    spdlog::get("logger")->info("create descriptor pool.");

    std::vector<vk::DescriptorPoolSize> pool_size = {
            vk::DescriptorPoolSize{
                    .type            = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = frames_in_flight,
            },
    };
    return device.createDescriptorPool(vk::DescriptorPoolCreateInfo{
            .maxSets = frames_in_flight,

            // pool 允许每个种类的 descriptor 各有多少个
            .poolSizeCount = (uint32_t) pool_size.size(),
            .pPoolSizes    = pool_size.data(),
    });
}


// TODO 这个应该和 uniform buffer 在一起出现
inline std::vector<vk::DescriptorSet> create_descriptor_set(const vk::Device &device,
                                                            const vk::DescriptorSetLayout &descriptor_set_layout,
                                                            const vk::DescriptorPool &descriptor_pool,
                                                            uint32_t frames_in_flight,
                                                            const std::vector<vk::Buffer> &descirptor_buffer_list)
{
    spdlog::get("logger")->info("create descriptor set.");

    if (descirptor_buffer_list.size() != frames_in_flight)
        throw std::runtime_error("descriptor buffer count error.");

    // 为每一帧都创建一个 descriptor set
    std::vector<vk::DescriptorSetLayout> layouts(frames_in_flight, descriptor_set_layout);
    std::vector<vk::DescriptorSet> descriptor_set_list;
    descriptor_set_list = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool     = descriptor_pool,
            .descriptorSetCount = (uint32_t) layouts.size(),
            .pSetLayouts        = layouts.data(),
    });

    // 将 descriptor set 与 uniform buffer 关联到一起
    for (size_t i = 0; i < frames_in_flight; ++i)
    {
        vk::DescriptorBufferInfo buffer_info = {
                .buffer = descirptor_buffer_list[i],
                .offset = 0,
                .range  = sizeof(UniformBufferObject),
        };

        vk::WriteDescriptorSet write_info = {
                .dstSet          = descriptor_set_list[i],
                .dstBinding      = 0,    // 写入 set 的哪个一 binding
                .dstArrayElement = 0,    // 如果 binding 对应数组，从第几个元素开始写
                .descriptorCount = 1,    // 写入几个数组元素
                .descriptorType  = vk::DescriptorType::eUniformBuffer,

                // buffer, image, image view 三选一
                .pImageInfo       = nullptr,
                .pBufferInfo      = &buffer_info,
                .pTexelBufferView = nullptr,
        };

        device.updateDescriptorSets({write_info}, {});
    }


    return descriptor_set_list;
}


inline vk::CommandPool create_command_pool(const vk::Device &device, const DeviceInfo &device_info)
{
    spdlog::get("logger")->info("create command pool.");
    vk::CommandPoolCreateInfo pool_create_info = {
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = device_info.graphics_queue_family_idx.value(),
    };
    return device.createCommandPool(pool_create_info);
}


/**
 * 更新 uniform buffer 的内容，更新 model 矩阵，让物体旋转起来
 */
inline void update_uniform_memory(const vk::Device &device, const SurfaceInfo &surface_info,
                                  const vk::DeviceMemory &uniform_memory)
{
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto cur_time = std::chrono::high_resolution_clock::now();
    float time    = std::chrono::duration<float, std::chrono::seconds::period>(cur_time - start_time).count();

    UniformBufferObject ubo = {
            .model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f)),
            .view  = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f)),
            .proj  = glm::perspective(glm::radians(45.f),
                                      (float) surface_info.extent.width / (float) surface_info.extent.height, 0.1f,
                                      10.f),
    };
    ubo.proj[1][1] *= -1.f;    // OpenGL 和 vulkan 的坐标系差异

    void *data = device.mapMemory(uniform_memory, 0, sizeof(ubo), {});
    std::memcpy(data, &ubo, sizeof(ubo));
    device.unmapMemory(uniform_memory);
}


inline std::vector<vk::CommandBuffer>
create_command_buffer(const vk::Device &device, const vk::CommandPool &command_pool, uint32_t frames_in_flight)
{
    spdlog::get("logger")->info("create command buffer.");
    vk::CommandBufferAllocateInfo allocate_info = {
            .commandPool        = command_pool,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<uint32_t>(frames_in_flight),
    };

    return device.allocateCommandBuffers(allocate_info);
}