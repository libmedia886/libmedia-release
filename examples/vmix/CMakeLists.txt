cmake_minimum_required(VERSION 3.16)
project(libmedia_vmix_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_vmix.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(vmix_tutorial main.c)
target_compile_definitions(vmix_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_vmix.so"
    MODULE_TYPES="VMIX"
    PRIMARY_TYPE="VMIX"
)
target_link_libraries(vmix_tutorial PRIVATE media dl pthread m)
set_target_properties(vmix_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
