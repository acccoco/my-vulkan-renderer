
# system header and lib
if (APPLE)
    add_compile_definitions(VK_ICD_FILENAMES=/usr/local/share/vulkan/icd.d/MoltenVK_icd.json)
    add_compile_definitions(VK_LAYER_PATH=/usr/local/share/vulkan/explicit_layer.d)
    # enable best practice validation layer
    add_compile_definitions(VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT;VALIDATION_CHECK_ENABLE_VENDOR_SPECIFIC_ARM)
endif ()


# third party
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(spdlog REQUIRED)
find_package(TinyGLTF REQUIRED HINTS ${CMAKE_SOURCE_DIR}/third_party/tinyGLTF)
find_package(TinyObjLoader REQUIRED HINTS ${CMAKE_SOURCE_DIR}/third_party/tinyobj)
set(LIBS
        glm::glm
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
        TinyGLTF
        TinyObjLoader
        )


# source files
set(SOURCE_FILES
        src/buffer.cpp
        src/env.cpp
        src/render_pass.cpp
        src/image.cpp
        src/framebuffer.cpp
        src/global.cpp
        src/vertex.cpp
        src/texture.cpp
        )


# static library
add_library(${PROJ_FRAMEWORK} STATIC ${SOURCE_FILES})
target_link_libraries(${PROJ_FRAMEWORK} PUBLIC ${LIBS})
target_include_directories(${PROJ_FRAMEWORK} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
