cmake_minimum_required(VERSION 3.16)
project(libmedia_tnr_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_tnr.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(tnr_tutorial main.c)
target_compile_definitions(tnr_tutorial PRIVATE
    TUTORIAL_PLUGIN_DIR="${LIBMEDIA_ROOT}/plugins"
    PLUGIN_BASENAME="libmedia_tnr.so"
    MODULE_TYPES="TNR_CL"
    PRIMARY_TYPE="TNR_CL"
)
target_link_libraries(tnr_tutorial PRIVATE media dl pthread m)
set_target_properties(tnr_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
