#include "../instance.hpp"


Hiss::Instance::Instance(const std::string &app_name, const std::vector<const char *> &extensions,
                         const std::vector<const char *>            &layers,
                         const vk::DebugUtilsMessengerCreateInfoEXT &debug_msger_info)
{
    vk::ApplicationInfo app_info = {
            .pApplicationName   = app_name.c_str(),
            .applicationVersion = VK_MAKE_VERSION(1, 1, 4),
            .pEngineName        = "Hiss 🥵 Engine",
            .engineVersion      = VK_MAKE_VERSION(5, 1, 4),
            .apiVersion         = VK_API_VERSION_1_3,
    };


    vk::InstanceCreateInfo instance_info = {
            /**
             * create 和 destroy 期间 validation layer 没有启用，
             * 可以通过这个方法来捕获这两个阶段的 event
             */
            .pNext = reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT *>(&debug_msger_info),

            /**
             * 表示 vulkan 除了枚举出默认的 physical device 外，
             * 还会枚举出符合 vulkan 可移植性的 physical device
             * 基于 metal 的 vulkan 需要这个
             */
            .flags                   = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
            .pApplicationInfo        = &app_info,
            .enabledLayerCount       = static_cast<uint32_t>(layers.size()),
            .ppEnabledLayerNames     = layers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
    };

    _instance = vk::createInstance(instance_info);
}


Hiss::Instance::~Instance() {
    _instance.destroy();
}
