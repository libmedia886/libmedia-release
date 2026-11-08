cmake_minimum_required(VERSION 3.16)
project(libmedia_vo_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_vo.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_library(plugin SHARED IMPORTED GLOBAL)
set_target_properties(plugin PROPERTIES
    IMPORTED_LOCATION "${PLUGIN_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(vo_tutorial main.c)
target_compile_definitions(vo_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_vo.so"
    MODULE_TYPES="VO"
    PRIMARY_TYPE="VO"
)
target_link_libraries(vo_tutorial PRIVATE media plugin dl pthread m)
set_target_properties(vo_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib;${LIBMEDIA_ROOT}/plugins"
)
