cmake_minimum_required(VERSION 3.16)
project(libmedia_thermal_lowlight_fusion_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")
set(PLUGIN_PATH "${LIBMEDIA_ROOT}/plugins/libmedia_thermal_lowlight_fusion.so")
if(NOT EXISTS "${PLUGIN_PATH}")
    message(FATAL_ERROR "Missing plugin: ${PLUGIN_PATH}")
endif()

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(thermal_lowlight_fusion_tutorial main.c)
target_compile_definitions(thermal_lowlight_fusion_tutorial PRIVATE
    PLUGIN_BASENAME="libmedia_thermal_lowlight_fusion.so"
    MODULE_TYPES="THERMAL_LOWLIGHT_FUSION_CL"
    PRIMARY_TYPE="THERMAL_LOWLIGHT_FUSION_CL"
)
target_link_libraries(thermal_lowlight_fusion_tutorial PRIVATE media "${PLUGIN_PATH}" dl pthread m)
set_target_properties(thermal_lowlight_fusion_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
