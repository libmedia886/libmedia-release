cmake_minimum_required(VERSION 3.16)
project(libmedia_vo_wbc_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_vo_wbc.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(vo_wbc_tutorial main.c)
target_compile_definitions(vo_wbc_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_vo_wbc.so"
    MODULE_TYPES="VO_WBC"
    PRIMARY_TYPE="VO_WBC"
)
target_link_libraries(vo_wbc_tutorial PRIVATE media dl pthread m)
set_target_properties(vo_wbc_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
