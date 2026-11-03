cmake_minimum_required(VERSION 3.16)
project(libmedia_load_all C)

set(LIBMEDIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "Path to the libmedia release directory")

add_library(media SHARED IMPORTED GLOBAL)
set_target_properties(media PROPERTIES
    IMPORTED_LOCATION "${LIBMEDIA_ROOT}/lib/libmedia.so"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBMEDIA_ROOT}/include"
)

add_executable(libmedia_load_all main.c)
target_link_libraries(libmedia_load_all PRIVATE media dl pthread m)
set_target_properties(libmedia_load_all PROPERTIES
    BUILD_RPATH "${LIBMEDIA_ROOT}/lib"
)
