cmake_minimum_required(VERSION 3.21)
project(cyrus VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_TESTING "Build all project tests" OFF)
option(STATIC_EXEC "Build a statically linked executable" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CyrusGuardInSourceBuilds)
include(CyrusSetDefaultInstallPrefix)
include(CyrusEnableIPO)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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
find_package(SampleRate REQUIRED CONFIG)
find_package(fmt REQUIRED CONFIG)
find_package(tl-expected REQUIRED CONFIG)

add_subdirectory(cyrus)
if (PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  add_subdirectory(tests)
endif ()


set(config_path "${CMAKE_CURRENT_BINARY_DIR}/cyrus/cyrusConfig.cmake")
set(version_path "${CMAKE_CURRENT_BINARY_DIR}/cyrus/cyrusConfigVersion.cmake")
set(targets_path "${CMAKE_CURRENT_BINARY_DIR}/cyrus/cyrusTargets.cmake")
set(config_pkg_dst "${CMAKE_INSTALL_DATAROOTDIR}/cmake/cyrus-${cyrus_VERSION}")
set(include_dir "${CMAKE_INSTALL_INCLUDEDIR}/cyrus-${cyrus_VERSION}")

install(TARGETS cyrus EXPORT cyrus_targets INCLUDES DESTINATION "${include_dir}")
export(EXPORT cyrus_targets NAMESPACE cyrus:: FILE "${targets_path}")
configure_package_config_file(cmake/cyrusConfig.cmake.in "${config_path}" INSTALL_DESTINATION "${config_pkg_dst}")
write_basic_package_version_file("${version_path}" VERSION ${cyrus_VERSION} COMPATIBILITY SameMajorVersion)
file(GLOB cyrus_headers LIST_DIRECTORIES false "cyrus/*.hpp")

install(EXPORT cyrus_targets DESTINATION "${config_pkg_dst}" NAMESPACE cyrus:: FILE cyrusTargets.cmake)
install(FILES "${config_path}" "${version_path}" DESTINATION "${config_pkg_dst}" COMPONENT Devel)
install(FILES ${cyrus_headers} DESTINATION "${include_dir}/cyrus" COMPONENT Devel)
