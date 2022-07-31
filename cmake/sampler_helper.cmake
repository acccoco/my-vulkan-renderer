

# compile shader
function(compile_shader)
    set(options)
    set(oneValueArgs
            TARGET_NAME
            SHADER_DIR)
    set(multiValueArgs
            SHADER_NAMES)
    cmake_parse_arguments(CS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if (NOT CS_TARGET_NAME
            OR NOT CS_SHADER_DIR)
        message(FATAL_ERROR "params error")
    endif ()


    set(SPV_FILES)
    set(SHADER_FILES)
    foreach (SHADER_NAME ${CS_SHADER_NAMES})
        set(SPV_FILE ${CS_SHADER_DIR}/${SHADER_NAME}.spv)
        set(SHADER_FILE ${CS_SHADER_DIR}/${SHADER_NAME})
        list(APPEND SPV_FILES ${SPV_FILE})
        list(APPEND SHADER_FILES ${SHADER_FILE})

        add_custom_command(
                OUTPUT ${SPV_FILE}
                COMMAND glslc ${SHADER_FILE} -o ${SPV_FILE}
                DEPENDS ${SHADER_FILE}
                VERBATIM
        )
    endforeach ()


    add_custom_target(${CS_TARGET_NAME}
            DEPENDS ${SPV_FILES}
            SOURCES ${SHADER_FILES}     # 让 IDE 的 source file 界面有这些文件
            )
endfunction()


function(add_sample)
    set(options)
    set(oneValueArgs
            TARGET_NAME
            SHADER_DIR)
    set(multiValueArgs
            SOURCES
            SHADER_NAMES)
    cmake_parse_arguments(SAMPLE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if (NOT SAMPLE_TARGET_NAME
            OR NOT SAMPLE_SHADER_DIR
            OR NOT SAMPLE_SOURCES)
        message(FATAL_ERROR "params error")
    endif ()

    compile_shader(
            TARGET_NAME ${SAMPLE_TARGET_NAME}.shader
            SHADER_DIR ${SAMPLE_SHADER_DIR}
            SHADER_NAMES ${SAMPLE_SHADER_NAMES}
    )
    add_executable(${SAMPLE_TARGET_NAME} ${SAMPLE_SOURCES})
    add_dependencies(${SAMPLE_TARGET_NAME} ${SAMPLE_TARGET_NAME}.shader)
    target_link_libraries(${SAMPLE_TARGET_NAME} ${PROJ_FRAMEWORK})
endfunction()