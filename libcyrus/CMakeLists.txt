add_library(cyrus_libcyrus STATIC cli.hpp cli.cpp device_probing.hpp device_probing.cpp)
add_library(cyrus::libcyrus ALIAS cyrus_libcyrus)
set_target_properties(cyrus_libcyrus PROPERTIES EXPORT_NAME libcyrus)
target_include_directories(cyrus_libcyrus PUBLIC "${PROJECT_SOURCE_DIR}")
