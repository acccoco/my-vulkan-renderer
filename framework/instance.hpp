#pragma once
#include "include_vk.hpp"


namespace Hiss
{

class Instance
{
public:
    Instance() = delete;
    Instance(const std::string &app_name, const std::vector<const char *> &extensions,
             const std::vector<const char *> &layers, const vk::DebugUtilsMessengerCreateInfoEXT &debug_msger_info);
    ~Instance();


    vk::Instance handle_get() { return _instance; }

private:
    vk::Instance _instance;
};
}    // namespace Hiss