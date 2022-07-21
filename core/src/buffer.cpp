#include "../buffer.hpp"
#include "../image.hpp"


void create_buffer(const vk::Device &device, const DeviceInfo &device_info, vk::DeviceSize size,
                   vk::BufferUsageFlags buffer_usage, vk::MemoryPropertyFlags memory_properties,
                   vk::Buffer &buffer, vk::DeviceMemory &buffer_memory)
{
    // 创建 buffer
    buffer = device.createBuffer({
            .size        = size,
            .usage       = buffer_usage,
            .sharingMode = vk::SharingMode::eExclusive,
    });


    // 分配 memory
    vk::MemoryRequirements mem_require = device.getBufferMemoryRequirements(buffer);
    std::optional<uint32_t> mem_type_idx =
            device_info.find_memory_type(mem_require, memory_properties);
    if (!mem_type_idx.has_value())
        throw std::runtime_error("no proper memory type for buffer, didn't allocate buffer.");
    buffer_memory = device.allocateMemory({
            .allocationSize  = mem_require.size,
            .memoryTypeIndex = mem_type_idx.value(),
    });


    // 绑定
    device.bindBufferMemory(buffer, buffer_memory, 0);
}


void create_uniform_buffer(const vk::Device &device, const DeviceInfo &device_info,
                           vk::Buffer &uniform_buffer, vk::DeviceMemory &uniform_memory)
{
    spdlog::get("logger")->info("create uniform buffer.");

    create_buffer(device, device_info, sizeof(UniformBufferObject),
                  vk::BufferUsageFlagBits::eUniformBuffer,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  uniform_buffer, uniform_memory);
}




vk::DescriptorPool create_descriptor_pool(const vk::Device &device, uint32_t frames_in_flight)
{
    spdlog::get("logger")->info("create descriptor pool.");

    std::vector<vk::DescriptorPoolSize> pool_size = {
            vk::DescriptorPoolSize{
                    .type            = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = frames_in_flight,
            },
            vk::DescriptorPoolSize{
                    .type            = vk::DescriptorType::eCombinedImageSampler,
                    .descriptorCount = frames_in_flight,
            }};

    return device.createDescriptorPool(vk::DescriptorPoolCreateInfo{
            .maxSets = frames_in_flight,

            // pool 允许每个种类的 descriptor 各有多少个
            .poolSizeCount = (uint32_t) pool_size.size(),
            .pPoolSizes    = pool_size.data(),
    });
}


vk::CommandPool create_command_pool(const vk::Device &device, const DeviceInfo &device_info)
{
    spdlog::get("logger")->info("create command pool.");
    vk::CommandPoolCreateInfo pool_create_info = {
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = device_info.graphics_queue_family_idx.value(),
    };
    return device.createCommandPool(pool_create_info);
}


void update_uniform_memory(const vk::Device &device, const SurfaceInfo &surface_info,
                           const vk::DeviceMemory &uniform_memory)
{
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto cur_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(cur_time - start_time)
                         .count();

    UniformBufferObject ubo = {
            .model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f),
                                 glm::vec3(0.f, 1.f, 0.f)),
            .view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f)),
            .proj = glm::perspective(glm::radians(45.f),
                                     (float) surface_info.extent.width /
                                             (float) surface_info.extent.height,
                                     0.1f, 10.f),
    };
    ubo.proj[1][1] *= -1.f;    // OpenGL 和 vulkan 的坐标系差异

    void *data = device.mapMemory(uniform_memory, 0, sizeof(ubo), {});
    std::memcpy(data, &ubo, sizeof(ubo));
    device.unmapMemory(uniform_memory);
}


std::vector<vk::CommandBuffer> create_command_buffer(const vk::Device &device,
                                                     const vk::CommandPool &command_pool,
                                                     uint32_t frames_in_flight)
{
    spdlog::get("logger")->info("create command buffer.");
    vk::CommandBufferAllocateInfo allocate_info = {
            .commandPool        = command_pool,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<uint32_t>(frames_in_flight),
    };

    return device.allocateCommandBuffers(allocate_info);
}


