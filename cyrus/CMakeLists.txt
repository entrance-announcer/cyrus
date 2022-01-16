add_executable(cyrus main.cpp)
target_link_libraries(cyrus PRIVATE cyrus_libcyrus audiofile::audiofile)
