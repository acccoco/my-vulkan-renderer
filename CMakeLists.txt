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

function(compile_shader src dst)
    add_custom_command(
            OUTPUT ${dst}
            COMMAND glslc ${src} -o ${dst}
            DEPENDS ${src}
            VERBATIM
    )
endfunction()

set(shader_dir ${CMAKE_SOURCE_DIR}/shader)

compile_shader(${shader_dir}/triangle.vert ${shader_dir}/triangle.vert.spv)
compile_shader(${shader_dir}/triangle.frag ${shader_dir}/triangle.frag.spv)

add_executable(VK main.cpp ${shader_dir}/triangle.vert.spv ${shader_dir}/triangle.frag.spv)
target_link_libraries(VK glfw glm::glm Vulkan::Vulkan spdlog::spdlog)

add_executable(test test.cpp)
target_link_libraries(test glfw glm::glm Vulkan::Vulkan spdlog::spdlog)

add_executable(new_vk vk.cpp ${shader_dir}/triangle.vert.spv ${shader_dir}/triangle.frag.spv)
target_link_libraries(new_vk glfw glm::glm Vulkan::Vulkan spdlog::spdlog)
