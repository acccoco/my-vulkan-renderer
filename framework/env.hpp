#pragma once

#include <set>
#include <vector>
#include <atomic>
#include <memory>
#include <optional>

#include "include_vk.hpp"
#include "global.hpp"

/**
 * physical device 以及 surface 的信息
 */
struct PhysicalInfo {
    vk::PhysicalDeviceProperties physical_device_properties;
    vk::PhysicalDeviceFeatures physical_device_features;
    vk::PhysicalDeviceMemoryProperties pdevice_mem_props;
    std::vector<vk::QueueFamilyProperties> queue_family_properties;
    std::vector<vk::ExtensionProperties> support_ext;

    vk::SurfaceCapabilitiesKHR surface_capability;
    std::vector<vk::SurfaceFormatKHR> surface_format_list;
    std::vector<vk::PresentModeKHR> present_mode_list;
    std::vector<uint32_t> grahics_queue_families;
    std::vector<uint32_t> present_queue_families;
    std::vector<uint32_t> transfer_queue_families;

    PhysicalInfo(const vk::PhysicalDevice &physical_device, const vk::SurfaceKHR &surface);
};


struct EnvSingleton {
    struct MyQueue {
        vk::Queue queue;
        uint32_t family_idx{};

        vk::Queue &operator()() { return queue; }
    };

    struct MyCmdPool {
        vk::CommandPool pool;
        MyQueue commit_queue;

        vk::CommandPool &operator()() { return pool; }
    };

#pragma region member
    vk::PhysicalDevice physical_device;
    vk::SurfaceKHR surface;

    /* physical device 和 surface 共同决定的一些属性 */
    std::shared_ptr<PhysicalInfo> info;

    vk::Device device;
    MyQueue graphics_queue;
    MyQueue present_queue;
    MyQueue transfer_queue;

    /* 该 command pool 创建的 command buffer 需要通过 graphics queue 提交 */
    MyCmdPool graphics_cmd_pool;

    vk::SurfaceFormatKHR present_format;
    vk::PresentModeKHR present_mode{};
    vk::Extent2D present_extent; /* surface 的 extent，以像素为单位 */


#pragma endregion


#pragma region public_method
    static void free(const vk::Instance &instance);
    static void init_once(const vk::Instance &instance);
    static void surface_recreate(const vk::Instance &instance);

    static std::shared_ptr<EnvSingleton> env()
    {
        assert(_env != nullptr);
        return _env;
    }

    static bool debug_has_init() { return _env != nullptr; }

    static std::optional<vk::Format> format_filter(const std::vector<vk::Format> &candidates,
                                                   vk::ImageTiling tiling,
                                                   vk::FormatFeatureFlags features);

    static vk::DeviceMemory mem_allocate(const vk::MemoryRequirements &mem_require,
                                         const vk::MemoryPropertyFlags &mem_prop);

    static vk::SampleCountFlagBits max_sample_cnt();

    /* depth format 中是否包含 stencil */
    static bool stencil_component_has(const vk::Format &format)
    {
        return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
    }

#pragma endregion

private:
    inline static std::shared_ptr<EnvSingleton> _env{nullptr};

    static vk::SurfaceKHR surface_create(const vk::Instance &instance, GLFWwindow *window);

    static bool physical_device_pick(const PhysicalInfo &info);

    static vk::Device device_create(const vk::PhysicalDevice &physical_device,
                                    const PhysicalInfo &physical_info,
                                    const std::vector<vk::DeviceQueueCreateInfo> &queue_info);

    static vk::CommandPool cmd_pool_create(const vk::Device &device, uint32_t queue_family_indx);

    static vk::SurfaceFormatKHR
    present_format_choose(const std::vector<vk::SurfaceFormatKHR> &format_list_);

    static vk::PresentModeKHR
    present_mode_choose(const std::vector<vk::PresentModeKHR> &present_mode_list_);

    static vk::Extent2D present_extent_choose(vk::SurfaceCapabilitiesKHR &capability_,
                                              GLFWwindow *window);
};
