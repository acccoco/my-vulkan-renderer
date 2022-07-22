#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_UNION_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <iostream>
#include <array>
#include <memory>


struct Test {

    int a = 3;
    ~Test() { std::cout << "des" << std::endl; }
};

int main()
{
    std::shared_ptr<Test> test{new Test()};
    test = std::make_shared<Test>();
    std::cout << "1" << std::endl;
    test = nullptr;
    std::cout << "2" << std::endl;
}