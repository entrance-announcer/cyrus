cmake_minimum_required(VERSION 3.21)
project(cyrus VERSION 0.0.1)

# guard against in-source builds
if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(
            FATAL_ERROR
            "In-source builds not allowed. Please make and use a build directory.")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# interprocedural/link-time optimization
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT err_msg)
if (ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
else ()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    message(NOTICE
            "Interprocedural linker optimization is not supported: ${err_msg}\n"
            "Continuing without it.")
endif ()

# default install prefix to Filesystem Hierarchy Standard's "add-on" path
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT
        AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows"
        AND PROJECT_IS_TOP_LEVEL)
    # todo: follow opt/ with provider, once registered with LANANA
    set(CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}" CACHE PATH
            "Base installation location." FORCE)
endif ()

list(APPEND CMAKE_PREFIX_PATH "${PROJECT_BINARY_DIR}")
set(CYRUS_DEFAULT_COMPILE_OPTIONS
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Weffc++>
        $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /WX>)

find_package(audiofile REQUIRED CONFIG)
find_package(fmt REQUIRED CONFIG)

enable_testing()
add_subdirectory(cyrus)
