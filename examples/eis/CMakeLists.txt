cmake_minimum_required(VERSION 3.16)
project(libmedia_eis_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_eis.so")
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

add_library(tpg SHARED IMPORTED GLOBAL)
set_target_properties(tpg PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/plugins/libmedia_tpg.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(eis_tutorial main.c)
target_compile_definitions(eis_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_eis.so"
    MODULE_TYPES="EIS"
    PRIMARY_TYPE="EIS"
)
target_link_libraries(eis_tutorial PRIVATE media plugin tpg dl pthread m)
set_target_properties(eis_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib;${LIBMEDIA_ROOT}/plugins"
)
