cmake_minimum_required(VERSION 3.16)
project(libmedia_wavelet_nr_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_wavelet_nr.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(wavelet_nr_tutorial main.c)
target_compile_definitions(wavelet_nr_tutorial PRIVATE
    TUTORIAL_PLUGIN_DIR="${LIBMEDIA_ROOT}/plugins"
    PLUGIN_BASENAME="libmedia_wavelet_nr.so"
    MODULE_TYPES="WAVELET_NR_CL"
    PRIMARY_TYPE="WAVELET_NR_CL"
)
target_link_libraries(wavelet_nr_tutorial PRIVATE media dl pthread m)
set_target_properties(wavelet_nr_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
