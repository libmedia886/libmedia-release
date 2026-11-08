cmake_minimum_required(VERSION 3.16)
project(libmedia_rga_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_rga.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(rga_tutorial main.c)
target_compile_definitions(rga_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_rga.so"
    MODULE_TYPES="RGA"
    PRIMARY_TYPE="RGA"
)
target_link_libraries(rga_tutorial PRIVATE media dl pthread m)
set_target_properties(rga_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
