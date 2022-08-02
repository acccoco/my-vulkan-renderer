#pragma once

#include <set>
#include <vector>
#include <atomic>
#include <memory>
#include <optional>

#include "include_vk.hpp"
#include "global.hpp"


namespace Hiss
{

/**
 * physical device 以及 surface 的信息
 */
struct DeviceInfo
{
    vk::PhysicalDeviceProperties           physical_device_properties;
    vk::PhysicalDeviceFeatures             physical_device_features;
    vk::PhysicalDeviceMemoryProperties     pdevice_mem_props;
    std::vector<vk::QueueFamilyProperties> queue_family_properties;
    std::vector<vk::ExtensionProperties>   support_ext;
    vk::SurfaceCapabilitiesKHR             surface_capability;
    std::vector<vk::SurfaceFormatKHR>      surface_format_list;
    std::vector<vk::PresentModeKHR>        present_mode_list;
    std::vector<uint32_t>                  grahics_queue_families;
    std::vector<uint32_t>                  present_queue_families;
    std::vector<uint32_t>                  transfer_queue_families;


    DeviceInfo(const vk::PhysicalDevice &physical_device, const vk::SurfaceKHR &surface);
};


struct MyQueue
{
    vk::Queue queue;
    uint32_t  family_idx{};

    vk::Queue &operator()() { return queue; }
};


struct MyCmdPool
{
    vk::CommandPool pool;
    MyQueue         commit_queue;

    vk::CommandPool &operator()() { return pool; }
};


struct Env
{
    vk::PhysicalDevice               physical_device;
    vk::SurfaceKHR                   surface;
    std::shared_ptr<DeviceInfo> info;    // physical device 和 surface 共同决定的一些属性
    vk::Device                       device;
    MyQueue                          graphics_queue;
    MyQueue                          present_queue;
    MyQueue                          transfer_queue;
    // TODO 创建一个 compute command pool
    MyCmdPool            graphics_cmd_pool;
    vk::SurfaceFormatKHR present_format;
    vk::PresentModeKHR   present_mode{};
    vk::Extent2D         present_extent; /* surface 的 extent，以像素为单位 */


    static void                      free(const vk::Instance &instance);
    static void                      init_once(const vk::Instance &instance);
    static void                      resize(const vk::Instance &instance);
    static std::shared_ptr<Env> env() { return _env; }


    static std::optional<vk::Format> format_filter(const std::vector<vk::Format> &candidates,
                                                        vk::ImageTiling tiling, vk::FormatFeatureFlags features);
    static vk::DeviceMemory               mem_allocate(const vk::MemoryRequirements  &mem_require,
                                                       const vk::MemoryPropertyFlags &mem_prop);
    static vk::SampleCountFlagBits        max_sample_cnt();


private:
    inline static std::shared_ptr<Env> _env{nullptr};


    static vk::Device      device_create(const vk::PhysicalDevice &physical_device, const DeviceInfo &physical_info,
                                         const std::vector<vk::DeviceQueueCreateInfo> &queue_info);
    static vk::SurfaceKHR  surface_create(const vk::Instance &instance, GLFWwindow *window);
    static bool            physical_device_pick(const DeviceInfo &info);
    static vk::CommandPool cmd_pool_create(const vk::Device &device, uint32_t queue_family_indx);
    static vk::SurfaceFormatKHR present_format_choose(const std::vector<vk::SurfaceFormatKHR> &format_list_);
    static vk::PresentModeKHR   present_mode_choose(const std::vector<vk::PresentModeKHR> &present_mode_list_);
    static vk::Extent2D         present_extent_choose(vk::SurfaceCapabilitiesKHR &capability_, GLFWwindow *window);
};
}    // namespace Hiss
