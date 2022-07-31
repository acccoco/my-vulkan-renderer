#include "../global.hpp"


/**
 * 用于查找函数地址的 dispatcher，vulkan hpp 提供了一个默认的
 * 这是变量定义的位置，vulkan.hpp 会通过 extern 引用这个全局变量
 */
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;


/**
 * 检查 layers 是否受支持，因为在 validation layer 启用之前没人报错，所以需要手动检查
 */
bool instance_layers_check(const std::vector<const char *> &layers)
{
    std::vector<vk::LayerProperties> layer_property_list = vk::enumerateInstanceLayerProperties();


    // 检查需要的 layer 是否受支持（笛卡尔积操作）
    for (const char *layer_needed: layers)
    {
        bool layer_found = false;
        for (const auto &layer_supported: layer_property_list)
        {
            if (strcmp(layer_needed, layer_supported.layerName) == 0)
            {
                layer_found = true;
                break;
            }
        }
        if (!layer_found)
            return false;
    }
    return true;
}


vk::Instance instance_create(const vk::DebugUtilsMessengerCreateInfoEXT &dbg_msger_create_info)
{
    LogStatic::logger()->info("create env.");


    // （这是可选的） 关于应用程序的信息，驱动可以根据这些信息对应用程序进行一些优化
    vk::ApplicationInfo app_info = {
            .pApplicationName   = "vk app",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_0,
    };


    // 所需的 vk 扩展
    std::vector<const char *> ext_list = {
            // validation layer 需要的扩展
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,

            // 基于 metal API 的 vulkan 实现需要这些扩展
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };
    /**
     * glfw 所需的 vk 扩展，window interface 相关的扩展
     * VK_KHR_surface: 这个扩展可以暴露出 VkSurfaceKHR 对象，glfw 可以读取这个对象
     */
    {
        uint32_t glfw_ext_cnt      = 0;
        const char **glfw_ext_list = glfwGetRequiredInstanceExtensions(&glfw_ext_cnt);
        for (int i = 0; i < glfw_ext_cnt; ++i)
            ext_list.push_back(glfw_ext_list[i]);
    }


    // 所需的 layers
    const std::vector<const char *> layer_list = {
            "VK_LAYER_KHRONOS_validation",
    };
    if (!instance_layers_check(layer_list))
        throw std::runtime_error("check layers: fail.");


    vk::InstanceCreateInfo instance_info = {
            /**
             * 一般情况下 debug messenger 只有在 env 创建后和销毁前才是有效的，
             * 将这个 create info 传递给 env 创建信息的 pNext 字段，
             * 可以在 env 的创建和销毁过程进行 debug
             */
            .pNext = reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT *>(
                    &dbg_msger_create_info),

            /// 表示 vulkan 除了枚举出默认的 physical device 外，
            /// 还会枚举出符合 vulkan 可移植性的 physical device
            /// 基于 metal 的 vulkan 需要这个
            .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,

            .pApplicationInfo = &app_info,

            // layers
            .enabledLayerCount   = (uint32_t) (layer_list.size()),
            .ppEnabledLayerNames = layer_list.data(),

            // extensions
            .enabledExtensionCount   = (uint32_t) ext_list.size(),
            .ppEnabledExtensionNames = ext_list.data(),
    };

    return vk::createInstance(instance_info);
}
