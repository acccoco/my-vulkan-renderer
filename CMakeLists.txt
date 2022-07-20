cmake_minimum_required(VERSION 3.22)
project(VK)


set(shader_dir ${CMAKE_SOURCE_DIR}/shader)


# compile options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")


# system header and lib
if (APPLE)
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)

    add_compile_definitions(VK_ICD_FILENAMES=/usr/local/share/vulkan/icd.d/MoltenVK_icd.json)
    add_compile_definitions(VK_LAYER_PATH=/usr/local/share/vulkan/explicit_layer.d)
    # enable best practice validation layer
    add_compile_definitions(VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT;VALIDATION_CHECK_ENABLE_VENDOR_SPECIFIC_ARM)
endif ()


# lib
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(spdlog REQUIRED)
find_package(TinyGLTF REQUIRED HINTS ./ext/tinyGLTF)
set(libs
        glm::glm
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
        TinyGLTF
        )


# compile shader
function(compile_shader src dst)
    add_custom_command(
            OUTPUT ${dst}
            COMMAND glslc ${src} -o ${dst}
            DEPENDS ${src}
            VERBATIM
    )
endfunction()
compile_shader(${shader_dir}/triangle.vert ${shader_dir}/triangle.vert.spv)
compile_shader(${shader_dir}/triangle.frag ${shader_dir}/triangle.frag.spv)


# 注入环境的头文件
configure_file(${CMAKE_SOURCE_DIR}/env.inl ${CMAKE_BINARY_DIR}/env.hpp)
include_directories(${CMAKE_BINARY_DIR})


# test
add_executable(test test.cpp)
target_link_libraries(test ${libs})


# main target
set(source
        ${shader_dir}/triangle.vert.spv
        ${shader_dir}/triangle.frag.spv
        vk.cpp
        buffer.cpp
        device.cpp
        render_pass.cpp
        )
add_executable(VK ${source})
target_link_libraries(VK ${libs})
