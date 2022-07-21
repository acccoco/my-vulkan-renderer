#pragma once


#include "./include_vk.hpp"
#include "./device.hpp"
#include "./tools.hpp"


struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 tex_coord;


    bool operator==(const Vertex &other) const
    {
        return pos == other.pos && color == other.color && tex_coord == other.tex_coord;
    }

    static std::vector<vk::VertexInputBindingDescription> binding_description_get()
    {
        return {
                vk::VertexInputBindingDescription{
                        /// 对应 binding array 的索引。
                        /// 因为只用了一个数组为 VAO 填充数据，因此 binding array 长度为 1，需要的索引就是0
                        .binding = 0,
                        .stride  = sizeof(Vertex),
                        // 是实例化的数据还是只是顶点数据
                        .inputRate = vk::VertexInputRate::eVertex,
                },
        };
    }


    /**
     * 第一个属性是 position，第二个属性是 color
     */
    static std::array<vk::VertexInputAttributeDescription, 3> attr_description_get()
    {
        return {vk::VertexInputAttributeDescription{
                        .location = 0,    // 就是 shader 中 in(location=0)
                        .binding  = 0,    // VAO 数据数组的第几个
                        .format   = vk::Format::eR32G32B32Sfloat,
                        .offset   = offsetof(Vertex, pos),
                },
                vk::VertexInputAttributeDescription{
                        .location = 1,
                        .binding  = 0,
                        .format   = vk::Format::eR32G32B32Sfloat,
                        .offset   = offsetof(Vertex, color),
                },
                vk::VertexInputAttributeDescription{
                        .location = 2,
                        .binding  = 0,
                        .format   = vk::Format::eR32G32Sfloat,
                        .offset   = offsetof(Vertex, tex_coord),
                }};
    }
};


/**
 * 为了让 Vertex 能够使用 hash 函数，将模版 specialize
 */
namespace std
{
    template<>
    struct hash<Vertex> {
        size_t operator()(Vertex const &vertex) const
        {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.tex_coord) << 1);
        }
    };
}


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