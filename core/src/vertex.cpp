#include "../vertex.hpp"
#include "../buffer.hpp"


void index_buffer_create(const std::vector<uint32_t> &indices, vk::Buffer &index_buffer,
                         vk::DeviceMemory &index_memory)
{
    LogStatic::logger()->info("create index buffer.");
    auto env = EnvSingleton::env();

    vk::DeviceSize buffer_size = sizeof(indices[0]) * indices.size();


    /* indices data -> stage buffer */
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    buffer_create(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  stage_buffer, stage_buffer_memory);

    void *data = env->device.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, indices.data(), (size_t) buffer_size);
    env->device.unmapMemory(stage_buffer_memory);


    /* stage buffer -> index buffer */
    buffer_create(buffer_size,
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, index_buffer, index_memory);
    OneTimeCmdBuffer one_time_cmd_buffer;
    one_time_cmd_buffer().copyBuffer(stage_buffer, index_buffer,
                                     {vk::BufferCopy{.size = buffer_size}});
    one_time_cmd_buffer.end();


    // free
    env->device.destroyBuffer(stage_buffer);
    env->device.freeMemory(stage_buffer_memory);
}


void vertex_buffer_create(const std::vector<Vertex> &vertices, vk::Buffer &vertex_buffer,
                          vk::DeviceMemory &vertex_memory)
{
    LogStatic::logger()->info("create vertex buffer.");
    auto env = EnvSingleton::env();

    vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();


    /* vertex data -> stage buffer */
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    buffer_create(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  stage_buffer, stage_buffer_memory);
    auto data = env->device.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, vertices.data(), (size_t) buffer_size);
    env->device.unmapMemory(stage_buffer_memory);


    /* stage buffer -> vertex buffer */
    buffer_create(buffer_size,
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, vertex_buffer, vertex_memory);
    {
        OneTimeCmdBuffer one_time_cmd_buffer;
        one_time_cmd_buffer().copyBuffer(stage_buffer, vertex_buffer,
                                         {vk::BufferCopy{.size = buffer_size}});
        one_time_cmd_buffer.end();
    }


    env->device.destroyBuffer(stage_buffer);
    env->device.free(stage_buffer_memory);
}
