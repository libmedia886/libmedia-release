cmake_minimum_required(VERSION 3.16)
project(libmedia_blend_pyr_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_blend_pyr.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(blend_pyr_tutorial main.c)
target_compile_definitions(blend_pyr_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_blend_pyr.so"
    MODULE_TYPES="BLEND_PYR"
    PRIMARY_TYPE="BLEND_PYR"
)
target_link_libraries(blend_pyr_tutorial PRIVATE media "${PLUGIN_PATH}" dl pthread m)
set_target_properties(blend_pyr_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
