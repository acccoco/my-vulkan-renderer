#include "../buffer.hpp"
#include "env.hpp"


void buffer_create(vk::DeviceSize size, vk::BufferUsageFlags buffer_usage,
              vk::MemoryPropertyFlags memory_properties, vk::Buffer &buffer,
              vk::DeviceMemory &buffer_memory)
{
    auto env = *Hiss::Env::env();

    // 创建 buffer
    buffer = env.device.createBuffer({
            .size        = size,
            .usage       = buffer_usage,
            .sharingMode = vk::SharingMode::eExclusive,
    });


    // 分配 memory
    vk::MemoryRequirements mem_require = env.device.getBufferMemoryRequirements(buffer);

    buffer_memory = Hiss::Env::mem_allocate(mem_require, memory_properties);


    // 绑定
    env.device.bindBufferMemory(buffer, buffer_memory, 0);
}


vk::DescriptorPool
create_descriptor_pool(uint32_t frames_in_flight)
{
    LogStatic::logger()->info("create descriptor pool.");

    std::vector<vk::DescriptorPoolSize> pool_size = {
            vk::DescriptorPoolSize{
                    .type            = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = frames_in_flight,
            },
            vk::DescriptorPoolSize{
                    .type            = vk::DescriptorType::eCombinedImageSampler,
                    .descriptorCount = frames_in_flight,
            }};

    return Hiss::Env::env()->device.createDescriptorPool(vk::DescriptorPoolCreateInfo{
            .maxSets = frames_in_flight,

            // pool 允许每个种类的 descriptor 各有多少个
            .poolSizeCount = (uint32_t) pool_size.size(),
            .pPoolSizes    = pool_size.data(),
    });
}
