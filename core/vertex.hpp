#pragma once

#include "include_vk.hpp"
#include "env.hpp"


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
}    // namespace std


/**
 * 创建 index buffer，并且把 indices 数据填入其中
 */
void index_buffer_create(const std::vector<uint32_t> &indices, vk::Buffer &index_buffer,
                         vk::DeviceMemory &index_memory);


/**
 * 创建 vertex buffer，将 vertex 数据填入其中
 */
void vertex_buffer_create(const std::vector<Vertex> &vertices, vk::Buffer &vertex_buffer,
                           vk::DeviceMemory &vertex_memory);