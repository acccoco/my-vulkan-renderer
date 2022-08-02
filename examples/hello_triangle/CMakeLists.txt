get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)


add_sample(
        TARGET_NAME ${FOLDER_NAME}
        SHADER_DIR ${PROJ_SHADER_DIR}
        SOURCES "hello_triangle.cpp" "hello_triangle.hpp"
        SHADER_NAMES triangle.vert triangle.frag
)

