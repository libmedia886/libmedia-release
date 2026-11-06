cmake_minimum_required(VERSION 3.16)
project(libmedia_vpss_16out C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(vpss_16out vpss_16out.c)
target_link_libraries(vpss_16out PRIVATE media dl pthread m)
set_target_properties(vpss_16out PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib;${LIBMEDIA_ROOT}/plugins"
)
