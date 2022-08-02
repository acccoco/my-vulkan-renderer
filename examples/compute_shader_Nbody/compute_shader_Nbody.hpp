#pragma once
#include <application.hpp>
#include "profile.hpp"


class ExampleComputeShaderNBody : public Hiss::ApplicationBase
{
    ExampleComputeShaderNBody()
        : ApplicationBase()
    {} /* graphics pass resource */
    struct Graphics
    {
        struct GraphcisUBO
        {
            glm::mat4 projection;
            glm::mat4 view;
            glm::vec2 screen_dim;
        } ubo{};


        vk::Buffer              uniform_buffer;
        vk::DescriptorSetLayout descriptor_set_layout;
        vk::DescriptorSet       descriptor_set;
        vk::PipelineLayout      pipeline_layout;
        vk::Pipeline            pipeline;
        vk::Semaphore           semaphore;    // 渲染完成的标记
    } graphics;


    /* compute pass resource */
    struct Compute
    {

        struct ComputeUBO
        {
            float   delta_time;
            int32_t particle_count;
        } ubo{};


        /* shader 中 specialization 的 constant 量，可以理解为 macro */
        struct MovementSpecializationData
        {
            uint32_t    workgroup_size;
            uint32_t    shared_data_size;
            const float gravity = 0.002f;
            const float power   = 0.75f;
            const float soften  = 0.05f;
        } movement_specialization_data;


        /* 一些配置信息 */
        uint32_t          num_particles{};
        uint32_t          work_group_size;
        uint32_t          work_group_cnt;
        uint32_t          shared_data_size;    // 单位是 sizeof(glm::vec4)
        const std::string shader_file_calculate = SHADER("compute_Nbody/calculate.comp");
        const std::string shader_file_integrate = SHADER("compute_Nbody/integrate.comp");


        const std::array<vk::DescriptorSetLayoutBinding, 2> layout_bindinds = {{
                {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                {1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute},
        }};


        vk::CommandBuffer       command_buffer;
        vk::Semaphore           semaphore;    // compute 过程完成的标记
        vk::DescriptorSetLayout descriptor_set_layout;
        vk::DescriptorSet       descriptor_set;
        vk::PipelineLayout      pipeline_layout;        // 两个 pipeline 的 layout 相同
        vk::Pipeline            pipeline_calculate;     // 计算质点受力，更新质点速度
        vk::Pipeline            pipeline_intergrate;    // 更新质点的位置
        vk::Buffer              uniform_buffer;
    } compute;


    struct Particle
    {
        glm::vec4 pos;    // xyz: position, w: mass
        glm::vec4 vel;    // xyz: velocity, w: uv coord
    };


    vk::DescriptorPool descriptor_pool;
    vk::Buffer         storage_buffer;


public:
    void prepare() override;
    void descriptor_pool_set();

    void graphics_prepare();
    // TODO 判断 storage buffer 是否是初次使用，否则需要 queue family transfer

    void compute_prepare();
    void compute_pipeline_create();
    void compute_descriptor_create();
    void compute_command_prepare();
};