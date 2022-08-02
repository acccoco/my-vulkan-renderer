#pragma once
#include "vk_common.hpp"
#include "window.hpp"


namespace Hiss
{

struct Queue
{
    vk::Queue queue;
    uint32_t  family_index;
};


struct CommandPool
{
    vk::CommandPool pool;
    uint32_t        queue_family_index;
};


class Device
{
private:
    vk::PhysicalDevice    _physical_device;
    std::vector<uint32_t> _graphics_queue_family_index;
    std::vector<uint32_t> _present_queue_family_index;
    std::vector<uint32_t> _compute_queue_family_index;
    vk::Device            _device;
    Queue                 _graphics_queue;
    Queue                 _present_queue;
    Queue                 _compute_queue;
    vk::SurfaceFormatKHR  _present_format;
    vk::PresentModeKHR    _present_mode{};
    vk::Extent2D          _present_extent;    // surface 的 extent，以像素为单位


    bool physical_device_pick(vk::Instance instance, vk::SurfaceKHR surface);
    void logical_device_create();
    void present_format_choose(vk::SurfaceKHR surface);
    void present_mode_choose(vk::SurfaceKHR surface);
    void surface_extent_choose(vk::SurfaceKHR surface, const Hiss::Window &window);


public:
    Device(vk::Instance instance, vk::SurfaceKHR surface, const Hiss::Window &window);
};
}    // namespace Hiss