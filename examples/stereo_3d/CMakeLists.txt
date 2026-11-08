cmake_minimum_required(VERSION 3.16)
project(libmedia_stereo_3d_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_stereo_3d.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(stereo_3d_tutorial main.c)
target_compile_definitions(stereo_3d_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_stereo_3d.so"
    MODULE_TYPES="STEREO_3D"
    PRIMARY_TYPE="STEREO_3D"
)
target_link_libraries(stereo_3d_tutorial PRIVATE media "${PLUGIN_PATH}" dl pthread m)
set_target_properties(stereo_3d_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
