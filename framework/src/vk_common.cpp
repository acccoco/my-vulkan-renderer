#include "../vk_common.hpp"


/**
 * 检查 layers 是否受支持，因为在 validation layer 启用之前没人报错，所以需要手动检查
 */
bool Hiss::instance_layers_check(const std::vector<const char *> &layers)
{
    std::vector<vk::LayerProperties> layer_property_list = vk::enumerateInstanceLayerProperties();


    /* 检查需要的 layer 是否受支持（笛卡尔积操作） */
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