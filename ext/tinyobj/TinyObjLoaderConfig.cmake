# -*- cmake -*-
# - Find TinyObjLoader
# TinyObjLoaderConfig.cmake 所在的文件夹就是 TinyObjLoader_DIR


FIND_PACKAGE ( PackageHandleStandardArgs )


FIND_FILE(source tiny_obj_loader.cpp PATHS ${TinyObjLoader_DIR})
FIND_FILE(header tiny_obj_loader.h PATHS ${TinyObjLoader_DIR})

IF (NOT source OR NOT header)
    MESSAGE ( FATAL_ERROR "Unable to find tiny_obj_loader")
ENDIF ()
message(STATUS "find TinyObjLoader success.")

add_library(TinyObjLoader STATIC ${TinyObjLoader_DIR}/tiny_obj_loader.cpp)
target_include_directories(TinyGLTF PUBLIC ${TinyObjLoader_DIR})
