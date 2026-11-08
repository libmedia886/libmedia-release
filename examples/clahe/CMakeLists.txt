cmake_minimum_required(VERSION 3.16)
project(libmedia_clahe_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_clahe.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(clahe_tutorial main.c)
target_compile_definitions(clahe_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_clahe.so"
    MODULE_TYPES="CLAHE"
    PRIMARY_TYPE="CLAHE"
)
target_link_libraries(clahe_tutorial PRIVATE media dl pthread m)
set_target_properties(clahe_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
