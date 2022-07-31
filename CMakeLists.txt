cmake_minimum_required(VERSION 3.22)
project(learn-VK)


# compile options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")



# folder and profiles
set(PROJ_SHADER_DIR ${CMAKE_SOURCE_DIR}/shader)
set(PROJ_ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)
configure_file(${CMAKE_SOURCE_DIR}/profile.inl ${CMAKE_BINARY_DIR}/in/profile.hpp)
include_directories(${CMAKE_BINARY_DIR}/in)
set(PROJ_FRAMEWORK framework)



include(cmake/sampler_helper.cmake)
add_subdirectory(framework)
add_subdirectory(examples)

