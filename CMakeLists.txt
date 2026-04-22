cmake_minimum_required(VERSION 3.21)
project(arcal-busmon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(WRITE "${CMAKE_BINARY_DIR}/.gitignore" "*\n")

# ---------------------------------------------------------------------------
# Locate arcal from its source + build tree.
# Pass -DARCAL_DIR=/path/to/arcal or set the ARCAL_DIR environment variable.
# When arcal gains proper CMake install/export support this becomes find_package(arcal).
# ---------------------------------------------------------------------------

set(ARCAL_DIR "$ENV{HOME}/repos/modular-af/arcal"
    CACHE PATH "arcal source tree root")
set(ARCAL_BUILD_DIR "${ARCAL_DIR}/build"
    CACHE PATH "arcal build directory")

foreach(_lib arcal arcal_externalizer_cdr arcal_externalizer_json)
    add_library(${_lib} SHARED IMPORTED)
    set_target_properties(${_lib} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ARCAL_DIR}/include"
    )
endforeach()

set_target_properties(arcal PROPERTIES
    IMPORTED_LOCATION "${ARCAL_BUILD_DIR}/libarcal.so")
set_target_properties(arcal_externalizer_cdr PROPERTIES
    IMPORTED_LOCATION "${ARCAL_BUILD_DIR}/externalizer/cdr/libarcal_externalizer_cdr.so")
set_target_properties(arcal_externalizer_json PROPERTIES
    IMPORTED_LOCATION "${ARCAL_BUILD_DIR}/externalizer/json/libarcal_externalizer_json.so")

find_package(CycloneDDS REQUIRED)

# Use mold linker if available.
find_program(MOLD_LINKER mold)
if(MOLD_LINKER)
    add_link_options(-fuse-ld=mold)
endif()

# ---------------------------------------------------------------------------
# arcal-busmon executable
# ---------------------------------------------------------------------------

add_executable(arcal-busmon
    src/main.cpp
    src/LogWriter.cpp
    src/TopicMonitor.cpp
    src/DecodeChain.cpp
    src/StreamPipe.cpp
)

target_include_directories(arcal-busmon PRIVATE
    src
    ${ARCAL_DIR}/src/dds   # arcal_payload.h + arcal_payload.hpp (idlc-generated)
)

target_link_libraries(arcal-busmon PRIVATE
    arcal
    arcal_externalizer_cdr
    arcal_externalizer_json
    CycloneDDS::ddsc
)

# Embed library paths in the binary so dlopen("libarcal_externalizer_*.so")
# resolves without LD_LIBRARY_PATH when running from the build tree.
set_target_properties(arcal-busmon PROPERTIES
    BUILD_RPATH
        "${ARCAL_BUILD_DIR};${ARCAL_BUILD_DIR}/externalizer/cdr;${ARCAL_BUILD_DIR}/externalizer/json"
)

install(TARGETS arcal-busmon RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Traffic generators — publish UCI messages to exercise the busmon UI.
# ---------------------------------------------------------------------------

add_subdirectory(pub)
