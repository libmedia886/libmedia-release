cmake_minimum_required(VERSION 3.16)
project(libmedia_tpg_tutorial C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)
add_library(media_tpg SHARED IMPORTED GLOBAL)
set_target_properties(media_tpg PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/plugins/libmedia_tpg.so"
)

add_executable(tpg_tutorial main.c)
target_link_libraries(tpg_tutorial PRIVATE media_tpg media dl pthread m)
set_target_properties(tpg_tutorial PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib;${LIBMEDIA_ROOT}/plugins"
)
