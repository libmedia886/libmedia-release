cmake_minimum_required(VERSION 3.16)
project(libmedia_edof_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_edof.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(edof_tutorial main.c)
target_compile_definitions(edof_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_edof.so"
    MODULE_TYPES="EDOF_CL"
    PRIMARY_TYPE="EDOF_CL"
)
target_link_libraries(edof_tutorial PRIVATE media "${PLUGIN_PATH}" dl pthread m)
set_target_properties(edof_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
