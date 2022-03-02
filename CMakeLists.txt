cmake_minimum_required(VERSION 3.21)
project(cyrus VERSION 0.0.1 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CyrusGuardInSourceBuilds)
include(CyrusSetDefaultInstallPrefix)
include(CyrusEnableIPO)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(BUILD_TESTING "Build all project tests" OFF)

enable_testing()
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

list(APPEND CMAKE_PREFIX_PATH "${PROJECT_BINARY_DIR}")
find_package(SndFile REQUIRED CONFIG)
find_package(fmt REQUIRED CONFIG)
find_package(tl-expected REQUIRED CONFIG)

add_subdirectory(cyrus)
if (PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  add_subdirectory(tests)
endif ()