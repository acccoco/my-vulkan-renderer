cmake_minimum_required(VERSION 3.22)
project(VK)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# system header and lib
if (APPLE)
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)

    # VK_ICD_FILENAMES=/usr/local/share/vulkan/icd.d/MoltenVK_icd.json;VK_LAYER_PATH=/usr/local/share/vulkan/explicit_layer.d
    add_compile_definitions(VK_ICD_FILENAMES=/usr/local/share/vulkan/icd.d/MoltenVK_icd.json)
    add_compile_definitions(VK_LAYER_PATH=/usr/local/share/vulkan/explicit_layer.d)
endif ()

# lib
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(spdlog REQUIRED)


add_executable(VK main.cpp )

target_link_libraries(VK glfw glm::glm Vulkan::Vulkan spdlog::spdlog)
