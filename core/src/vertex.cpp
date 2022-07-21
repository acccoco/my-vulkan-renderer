#include "../vertex.hpp"
#include "../buffer.hpp"


void create_index_buffer(const vk::Device &device, const DeviceInfo &device_info,
                         const vk::CommandPool &cmd_pool, const vk::Queue &transfer_queue,
                         const std::vector<uint32_t> &indices, vk::Buffer &index_buffer,
                         vk::DeviceMemory &index_memory)
{
    spdlog::get("logger")->info("create index buffer.");

    vk::DeviceSize buffer_size = sizeof(indices[0]) * indices.size();


    // indices data -> stage buffer
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    create_buffer(device, device_info, buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  stage_buffer, stage_buffer_memory);

    void *data = device.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, indices.data(), (size_t) buffer_size);
    device.unmapMemory(stage_buffer_memory);


    // stage buffer -> index buffer
    create_buffer(device, device_info, buffer_size,
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, index_buffer, index_memory);
    OneTimeCmdBuffer one_time_cmd_buffer(device, transfer_queue, cmd_pool);
    one_time_cmd_buffer().copyBuffer(stage_buffer, index_buffer,
                                     {vk::BufferCopy{.size = buffer_size}});
    one_time_cmd_buffer.end();


    // free
    device.destroyBuffer(stage_buffer);
    device.freeMemory(stage_buffer_memory);
}


void create_vertex_buffer_(const vk::Device &device, const DeviceInfo &device_info,
                           const vk::CommandPool &cmd_pool, const vk::Queue &transfer_queue,
                           const std::vector<Vertex> &vertices, vk::Buffer &vertex_buffer,
                           vk::DeviceMemory &vertex_memory)
{
    spdlog::get("logger")->info("create vertex buffer.");

    vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();


    // vertex data -> stage buffer
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    create_buffer(device, device_info, buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  stage_buffer, stage_buffer_memory);
    // params: device_memory_obj, offset, size, flags
    auto data = device.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, vertices.data(), (size_t) buffer_size);
    device.unmapMemory(stage_buffer_memory);


    // stage buffer -> vertex buffer
    create_buffer(device, device_info, buffer_size,
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, vertex_buffer, vertex_memory);
    {
        OneTimeCmdBuffer one_time_cmd_buffer(device, transfer_queue, cmd_pool);
        one_time_cmd_buffer().copyBuffer(stage_buffer, vertex_buffer,
                                         {vk::BufferCopy{.size = buffer_size}});
        one_time_cmd_buffer.end();
    }


    // 销毁 stage buffer 以及 memory
    device.destroyBuffer(stage_buffer);
    device.free(stage_buffer_memory);
}

