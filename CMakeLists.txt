cmake_minimum_required(VERSION 3.21)
project(arcal-busmon-tools VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(WRITE "${CMAKE_BINARY_DIR}/.gitignore" "*\n")

# ---------------------------------------------------------------------------
# Locate arcal from its source + build tree.
# Pass -DARCAL_DIR=/path/to/arcal or set the ARCAL_DIR environment variable.
# ---------------------------------------------------------------------------

set(ARCAL_DIR "$ENV{HOME}/repos/modular-af/arcal"
    CACHE PATH "arcal source tree root")
set(ARCAL_BUILD_DIR "${ARCAL_DIR}/build"
    CACHE PATH "arcal build directory")

add_library(arcal SHARED IMPORTED)
set_target_properties(arcal PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ARCAL_DIR}/include"
    IMPORTED_LOCATION             "${ARCAL_BUILD_DIR}/libarcal.so"
)

find_program(MOLD_LINKER mold)
if(MOLD_LINKER)
    add_link_options(-fuse-ld=mold)
endif()

add_subdirectory(pub)
