#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_UNION_CONSTRUCTORS
#include <vulkan/vulkan.hpp>


const int* foo(const vk::ArrayProxyNoTemporaries<int> & arr) {
    return arr.data();
}

int main() {

}