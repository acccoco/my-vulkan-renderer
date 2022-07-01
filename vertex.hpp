#pragma once
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


struct Vertex {
    glm::vec2 position;
    glm::vec3 color;

    static vk::VertexInputBindingDescription get_binding_description_()
    {
        return {
                /// 对应 binding array 的索引。
                /// 因为只用了一个数组为 VAO 填充数据，因此 binding array 长度为 1，需要的索引就是0
                .binding = 0,
                .stride  = sizeof(Vertex),
                // 是实例化的数据还是只是顶点数据
                .inputRate = vk::VertexInputRate::eVertex,
        };
    }


    /**
     * 第一个属性是 position，第二个属性是 color
     */
    static std::array<vk::VertexInputAttributeDescription, 2> get_attr_descriptions_()
    {
        return {vk::VertexInputAttributeDescription{
                        .location = 0,    // 就是 shader 中 in(location=0)
                        .binding  = 0,    // VAO 数据数组的第几个
                        .format   = vk::Format::eR32G32Sfloat,
                        .offset   = offsetof(Vertex, position),
                },
                vk::VertexInputAttributeDescription{
                        .location = 1,
                        .binding  = 0,
                        .format   = vk::Format::eR32G32B32Sfloat,
                        .offset   = offsetof(Vertex, color),
                }};
    }
};


//
std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0,
};


// 三角形的顶点数据
const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
};