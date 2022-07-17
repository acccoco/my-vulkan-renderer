# -*- cmake -*-
# - Find TinyGLTF

# TinyGLTF_INCLUDE_DIR      TinyGLTF's include directory


FIND_PACKAGE ( PackageHandleStandardArgs )

SET ( TinyGLTF_INCLUDE_DIR "${TinyGLTF_DIR}/../include" CACHE STRING "TinyGLTF include directory")
set ( TinyGLFT_SRC_DIR ${TinyGLTF_DIR}/../src)

FIND_FILE ( TinyGLTF_HEADER tiny_gltf.h PATHS ${TinyGLTF_INCLUDE_DIR} )

IF (NOT TinyGLTF_HEADER)
  MESSAGE ( FATAL_ERROR "Unable to find tiny_gltf.h, TinyGLTF_INCLUDE_DIR = ${TinyGLTF_INCLUDE_DIR}")
ENDIF ()

add_library(TinyGLTF STATIC ${TinyGLFT_SRC_DIR}/tiny_gltf.cpp)
target_include_directories(TinyGLTF PUBLIC ${TinyGLTF_INCLUDE_DIR})
