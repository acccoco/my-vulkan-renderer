#pragma once

#include "./include_vk.hpp"
#include "./render_pass.hpp"
#include "env.hpp"
#include <tiny_obj_loader.h>
#include <unordered_map>


class TestModel
{
    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;

    vk::Buffer _vertex_buffer;
    vk::DeviceMemory _vertex_mem;
    vk::Buffer _index_buffer;
    vk::DeviceMemory _index_mem;

    std::string MODEL_PATH   = std::string(model_dir) + "/viking_room.obj";
    std::string TEXTURE_PATH = std::string(tex_dir) + "/viking_room.png";


public:
    void model_load(const Env &env)
    {
        tinyobj::attrib_t attr;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        std::string err;

        if (!tinyobj::LoadObj(&attr, &shapes, &materials, &err, MODEL_PATH.c_str()))
            throw std::runtime_error(err);


        /* TinyObjLoader 不会重复利用模型中的顶点，这里手动重用.
         * Vertex hash 需要实现两个函数：equality test, hash calculation */
        std::unordered_map<Vertex, uint32_t> uniq_vertices;

        for (const auto &shape: shapes)
        {
            for (const auto &index: shape.mesh.indices)
            {
                Vertex vertex = {
                        .pos       = {attr.vertices[3 * index.vertex_index + 0],
                                      attr.vertices[3 * index.vertex_index + 1],
                                      attr.vertices[3 * index.vertex_index + 2]},
                        .color     = {1.f, 1.f, 1.f},
                        .tex_coord = {attr.texcoords[2 * index.texcoord_index + 0],
                                      1.f - attr.texcoords[2 * index.texcoord_index + 1]},
                };

                if (uniq_vertices.count(vertex) == 0)
                {
                    uniq_vertices[vertex] = static_cast<uint32_t>(_vertices.size());
                    _vertices.push_back(vertex);
                }

                _indices.push_back(uniq_vertices[vertex]);
            }
        }

        create_vertex_buffer_(env.device, env.device_info, env.cmd_pool, env.graphics_queue,
                              _vertices, _vertex_buffer, _vertex_mem);
        create_index_buffer(env.device, env.device_info, env.cmd_pool, env.graphics_queue, _indices,
                            _index_buffer, _index_mem);
    }


    vk::Buffer &vertex_buffer() { return _vertex_buffer; }
    vk::Buffer &index_buffer() { return _index_buffer; }
    uint32_t index_cnt() { return _indices.size(); }


    void resource_free(const Env &env)
    {
        env.device.free(_vertex_mem);
        env.device.free(_index_mem);
        env.device.destroy(_vertex_buffer);
        env.device.destroy(_index_buffer);
    }
};