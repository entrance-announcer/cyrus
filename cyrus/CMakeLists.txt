# private library for the main executable, allowing unit testing
add_library(cyrus_cyruslib STATIC
        cli.hpp cli.cpp
        device_probing.hpp device_probing.cpp
        sample_conversions.hpp
        write_audio.hpp write_audio.cpp
        user_message_exception.hpp user_message_exception.cpp
        cyrus_main.hpp cyrus_main.cpp
        audio_signal.hpp)
target_include_directories(cyrus_cyruslib PUBLIC "${PROJECT_SOURCE_DIR}")
target_compile_options(cyrus_cyruslib PRIVATE "${CYRUS_DEFAULT_COMPILE_OPTIONS}")
target_link_libraries(cyrus_cyruslib PRIVATE fmt::fmt SndFile::sndfile)

# main cyrus executable
add_executable(cyrus main.cpp)
target_compile_options(cyrus PRIVATE "${CYRUS_DEFAULT_COMPILE_OPTIONS}")
target_link_libraries(cyrus PRIVATE cyrus_cyruslib)
