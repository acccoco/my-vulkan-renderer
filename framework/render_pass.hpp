#pragma once
#include "include_vk.hpp"
#include "env.hpp"
#include "tools.hpp"
#include "vertex.hpp"
#include "framebuffer.hpp"


struct UniformBufferObject {
    /* c++ 的 struct 和 SPIR-V 的 struct 的对应，因此需要考虑 alignment 的问题 */
    /* align test */
    alignas(16) glm::vec3 _foo;
    alignas(16) glm::vec3 _foo2;

    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};


vk::RenderPass render_pass_create(const FramebufferLayout_temp &framebuffer_layout);


vk::Pipeline pipeline_create(const vk::PipelineLayout &pipeline_layout,
                             const vk::RenderPass &render_pass);


vk::DescriptorSetLayout descriptor_set_layout_create();


vk::PipelineLayout
pipeline_layout_create(const std::vector<vk::DescriptorSetLayout> &descriptor_set_layout);


std::vector<vk::DescriptorSet>
create_descriptor_set(const vk::DescriptorSetLayout &descriptor_set_layout,
                      const vk::DescriptorPool &descriptor_pool, uint32_t frames_in_flight,
                      std::array<vk::Buffer, 2U> uniform_buffer_list,
                      const vk::ImageView &tex_img_view, const vk::Sampler &tex_sampler);


// TODO pipeline 的配置是 data，是信息。应该是声明式的，而不是命令式的
struct Pipeline {
    std::vector<vk::DescriptorSetLayoutBinding> descriptor_set_layout = {
            {.binding         = 0,
             .descriptorType  = vk::DescriptorType::eUniformBuffer,
             .descriptorCount = 1, /* 大于 1 表示数组 */
             .stageFlags      = vk::ShaderStageFlagBits::eVertex},
            {.binding            = 1,
             .descriptorType     = vk::DescriptorType::eCombinedImageSampler,
             .descriptorCount    = 1,
             .stageFlags         = vk::ShaderStageFlagBits::eFragment,
             .pImmutableSamplers = nullptr}};
};