#pragma once
#include "include_vk.hpp"

namespace Hiss
{

/* depth format 中是否包含 stencil */
inline static bool stencil_component_has(const vk::Format &format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}


bool instance_layers_check(const std::vector<const char *> &layers);

}    // namespace Hiss