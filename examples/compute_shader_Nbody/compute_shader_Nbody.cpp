#include "compute_shader_Nbody.hpp"
#include "application.hpp"


APP_RUN(ExampleComputeShaderNBody);


void ExampleComputeShaderNBody::prepare()
{
    Hiss::ApplicationBase::prepare();

    vk::PhysicalDevice           pd;     // FIXME
    vk::Device                   d;      // FIXME
    vk::PhysicalDeviceProperties pdp;    // FIXME
}


void ExampleComputeShaderNBody::compute_prepare()
{
    vk::PhysicalDeviceProperties pdp;    // FIXME
    vk::Device                   d;      // FIXME
    vk::CommandPool              p;      // FIXME
    vk::Queue                    q;      // FIXME


    /* workgroup 相关的数据 */
    compute.work_group_size  = std::max<uint32_t>(256, pdp.limits.maxComputeWorkGroupSize[0]);
    compute.shared_data_size = std::max<uint32_t>(1024, pdp.limits.maxComputeSharedMemorySize / sizeof(glm::vec4));
    compute.work_group_cnt   = compute.num_particles / compute.work_group_size;

    compute.movement_specialization_data.workgroup_size   = compute.work_group_size;
    compute.movement_specialization_data.shared_data_size = compute.shared_data_size;


    compute_descriptor_create();
    compute_pipeline_create();


    /* 创建 command buffer */
    compute.command_buffer = d.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
            .commandPool        = p,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    })[0];


    /* 创建 semaphore，并设为 signaled 状态 */
    compute.semaphore = d.createSemaphore({});
    q.submit(vk::SubmitInfo{.signalSemaphoreCount = 1, .pSignalSemaphores = &compute.semaphore});
    q.waitIdle();


    /* 录制 command buffer */
    compute_command_prepare();


    // TODO 或许这里要进行 queue family owner transfer，看应用的实现情况
}


void ExampleComputeShaderNBody::graphics_prepare() {}


void ExampleComputeShaderNBody::descriptor_pool_set()
{
    vk::Device d;    // FIXME
    /**
     * compute shader: 1 uniform buffer, 1 storage buffer
     * vertex shader: 1 uniform buffer
     * fragment shader: 2 sampler
     */
    std::array<vk::DescriptorPoolSize, 3> pool_size = {{
            {vk::DescriptorType::eUniformBuffer, 2},
            {vk::DescriptorType::eStorageBuffer, 1},
            {vk::DescriptorType::eCombinedImageSampler, 2},
    }};

    descriptor_pool = d.createDescriptorPool(vk::DescriptorPoolCreateInfo{
            .maxSets       = 2,    // 最多可以分配 2 个 descriptor set
            .poolSizeCount = static_cast<uint32_t>(pool_size.size()),
            .pPoolSizes    = pool_size.data(),
    });
}


/**
 * 创建两个 compute pipeline
 */
void ExampleComputeShaderNBody::compute_pipeline_create()
{
    vk::Device                    d;    // FIXME
    vk::ComputePipelineCreateInfo pipeline_info;


    /* pipeline layout */
    compute.descriptor_set_layout = d.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(compute.layout_bindinds.size()),
            .pBindings    = compute.layout_bindinds.data(),
    });
    compute.pipeline_layout       = d.createPipelineLayout(vk::PipelineLayoutCreateInfo{
                  .setLayoutCount = 1,
                  .pSetLayouts    = &compute.descriptor_set_layout,
    });
    pipeline_info.layout          = compute.pipeline_layout;


    /* 1st pipeline: calculate */
    {
        /* shader, and specialization constant */
        pipeline_info.stage = shader_load(compute.shader_file_calculate, vk::ShaderStageFlagBits::eCompute);
        std::array<vk::SpecializationMapEntry, 5> specialization_entries = {{
                {0, offsetof(Compute::MovementSpecializationData, workgroup_size),
                 sizeof(compute.movement_specialization_data.workgroup_size)},
                {1, offsetof(Compute::MovementSpecializationData, shared_data_size),
                 sizeof(compute.movement_specialization_data.shared_data_size)},
                {2, offsetof(Compute::MovementSpecializationData, gravity),
                 sizeof(compute.movement_specialization_data.gravity)},
                {3, offsetof(Compute::MovementSpecializationData, power),
                 sizeof(compute.movement_specialization_data.power)},
                {4, offsetof(Compute::MovementSpecializationData, soften),
                 sizeof(compute.movement_specialization_data.soften)},
        }};

        vk::SpecializationInfo specialization_info = {
                static_cast<uint32_t>(specialization_entries.size()),
                specialization_entries.data(),
                sizeof(compute.movement_specialization_data),
                &compute.movement_specialization_data,
        };
        pipeline_info.stage.pSpecializationInfo = &specialization_info;

        /* create pipeline */
        vk::Result result;
        std::tie(result, compute.pipeline_calculate) = d.createComputePipeline(VK_NULL_HANDLE, pipeline_info);
        if (result != vk::Result::eSuccess)
            throw std::runtime_error("fail to create compute pipeline: calculate.");
    }


    /* 2nd pipeline: integrate */
    {
        /* shader, and specialization */
        pipeline_info.stage = shader_load(compute.shader_file_integrate, vk::ShaderStageFlagBits::eCompute);
        vk::SpecializationMapEntry specialization_entry = {0, 0, sizeof(compute.work_group_size)};
        vk::SpecializationInfo     specialization_info  = {
                1,
                &specialization_entry,
                sizeof(compute.work_group_size),
                &compute.work_group_size,
        };
        pipeline_info.stage.pSpecializationInfo = &specialization_info;

        /* create pipeline */
        vk::Result result;
        std::tie(result, compute.pipeline_intergrate) = d.createComputePipeline(VK_NULL_HANDLE, pipeline_info);
    }
}


/**
 * 创建 descriptor set，并将其绑定到 uniform buffer 和 storage buffer
 */
void ExampleComputeShaderNBody::compute_descriptor_create()
{
    vk::Device d;    // FIXME

    compute.descriptor_set = d.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool     = descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &compute.descriptor_set_layout,
    })[0];


    /* 将 descriptor 和 buffer 绑定起来 */
    vk::DescriptorBufferInfo storage_buffer_info = {storage_buffer, 0, VK_WHOLE_SIZE};
    vk::DescriptorBufferInfo uniform_buffer_info = {compute.uniform_buffer, 0, VK_WHOLE_SIZE};

    std::array<vk::WriteDescriptorSet, 2> descriptor_writes = {{
            {
                    .dstSet          = compute.descriptor_set,
                    .dstBinding      = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType  = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo     = &storage_buffer_info,
            },
            {
                    .dstSet          = compute.descriptor_set,
                    .dstBinding      = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType  = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo     = &storage_buffer_info,
            },
    }};
    d.updateDescriptorSets(descriptor_writes, {});
}


void ExampleComputeShaderNBody::compute_command_prepare()
{
    uint32_t graphics_queue_idx = 114;    // FIXME
    uint32_t compute_queue_idx  = 514;
    uint32_t storage_size       = 1919810;    // 是否仿照 sample 的写法，构造一个 obj

    compute.command_buffer.begin(vk::CommandBufferBeginInfo{});


    /* queue family owner transfer: acquire */
    if (compute_queue_idx != graphics_queue_idx)
    {
        vk::BufferMemoryBarrier2 barrier = {
                .srcStageMask        = vk::PipelineStageFlagBits2::eVertexShader,
                .srcAccessMask       = {},    // acquire 中的这个字段被忽略
                .dstStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
                .dstAccessMask       = vk::AccessFlagBits2::eShaderWrite,
                .srcQueueFamilyIndex = graphics_queue_idx,
                .dstQueueFamilyIndex = compute_queue_idx,
                .buffer              = storage_buffer,
                .offset              = 0,
                .size                = storage_size,
        };
        compute.command_buffer.pipelineBarrier2(vk::DependencyInfo{
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &barrier,
        });
    }


    /* 1st pass: 计算受力，更新速度 */
    compute.command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute.pipeline_calculate);
    compute.command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute.pipeline_layout, 0,
                                              {compute.descriptor_set}, nullptr);
    compute.command_buffer.dispatch(compute.num_particles / compute.work_group_size, 1, 1);


    /* 在 1st 和 2nd 之间插入 memory barrier */
    vk::BufferMemoryBarrier2 memory_barrier = {
            .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
            .srcAccessMask       = vk::AccessFlagBits2::eShaderWrite,
            .dstStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask       = vk::AccessFlagBits2::eShaderRead,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = storage_buffer,
            .offset              = 0,
            .size                = storage_size,
    };
    compute.command_buffer.pipelineBarrier2(vk::DependencyInfo{
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers    = &memory_barrier,
    });


    /* 2nd pass: 更新质点的位置 */
    compute.command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute.pipeline_intergrate);
    compute.command_buffer.dispatch(compute.num_particles / compute.work_group_size, 1, 1);
    // 两个 pass descriptor 是一样的，因此无需再次 bingding


    /* queue family owner transfer: release */
    if (graphics_queue_idx != compute_queue_idx)
    {
        vk::BufferMemoryBarrier2 barrier = {
                .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
                .srcAccessMask       = vk::AccessFlagBits2::eShaderWrite,
                .dstStageMask        = vk::PipelineStageFlagBits2::eVertexShader,
                .dstAccessMask       = {},    // release 中的这个字段被忽略
                .srcQueueFamilyIndex = compute_queue_idx,
                .dstQueueFamilyIndex = graphics_queue_idx,
                .buffer              = storage_buffer,
                .offset              = 0,
                .size                = storage_size,
        };
        compute.command_buffer.pipelineBarrier2(vk::DependencyInfo{
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &barrier,
        });


        compute.command_buffer.end();
    }
}
