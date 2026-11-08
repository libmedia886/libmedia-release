cmake_minimum_required(VERSION 3.16)
project(libmedia_raw_wb_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_raw_wb.so")
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

add_executable(raw_wb_tutorial main.c)
target_compile_definitions(raw_wb_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_raw_wb.so"
    MODULE_TYPES="RAW_WB"
    PRIMARY_TYPE="RAW_WB"
)
target_link_libraries(raw_wb_tutorial PRIVATE media plugin dl pthread m)
set_target_properties(raw_wb_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib;${LIBMEDIA_ROOT}/plugins"
)
