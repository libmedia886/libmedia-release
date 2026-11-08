cmake_minimum_required(VERSION 3.16)
project(libmedia_vi_mipi C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_library(media_vi SHARED IMPORTED GLOBAL)
set_target_properties(media_vi PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/plugins/libmedia_vi.so"
)

add_executable(vi_mipi vi_mipi.c)
target_link_libraries(vi_mipi PRIVATE media_vi media dl pthread m)
set_target_properties(vi_mipi PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib;${LIBMEDIA_ROOT}/plugins"
)
