#pragma once


#include "./include_vk.hpp"
#include "./device.hpp"
#include "./tools.hpp"
#include "./vertex.hpp"


struct UniformBufferObject {
    /* c++ 的 struct 和 SPIR-V 的 struct 的对应，因此需要考虑 alignment 的问题 */
    /* align test */
    alignas(16) glm::vec3 _foo;
    alignas(16) glm::vec3 _foo2;

    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};


vk::RenderPass create_render_pass(const Env &env);


vk::Pipeline create_pipeline(const vk::Device &device, const SurfaceInfo &surface_info,
                             const vk::PipelineLayout &pipeline_layout,
                             const vk::RenderPass &render_pass);


vk::DescriptorSetLayout create_descriptor_set_layout(const vk::Device &device);


vk::PipelineLayout
create_pipelien_layout(const vk::Device &device,
                       const std::vector<vk::DescriptorSetLayout> &descriptor_set_layout);


std::vector<vk::DescriptorSet>
create_descriptor_set(const vk::Device &device,
                      const vk::DescriptorSetLayout &descriptor_set_layout,
                      const vk::DescriptorPool &descriptor_pool, uint32_t frames_in_flight,
                      const std::vector<vk::Buffer> &uniform_buffer_list,
                      const vk::ImageView &tex_img_view, const vk::Sampler &tex_sampler);