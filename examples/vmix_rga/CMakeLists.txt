cmake_minimum_required(VERSION 3.16)
project(libmedia_vmix_rga_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_vmix_rga.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(vmix_rga_tutorial main.c)
target_compile_definitions(vmix_rga_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_vmix_rga.so"
    MODULE_TYPES="VMIX_RGA"
    PRIMARY_TYPE="VMIX_RGA"
)
target_link_libraries(vmix_rga_tutorial PRIVATE media dl pthread m)
set_target_properties(vmix_rga_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
