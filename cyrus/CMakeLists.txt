# private library for the main executable, allowing unit testing
add_library(cyrus_objects OBJECT
  cli.hpp cli.cpp
  device_probing.hpp device_probing.cpp
  sample_conversions.hpp
  signal_conversions.hpp
  write_audio.hpp write_audio.cpp
  cyrus_main.hpp cyrus_main.cpp
  audio_signal.hpp
  try.hpp
  )
target_include_directories(cyrus_objects PUBLIC "${PROJECT_SOURCE_DIR}")
target_compile_options(cyrus_objects PRIVATE "${CYRUS_DEFAULT_COMPILE_OPTIONS}")
target_link_libraries(cyrus_objects
  PUBLIC
  tl::expected
  PRIVATE
  fmt::fmt
  SndFile::sndfile
  SampleRate::samplerate)

# main cyrus executable
add_executable(cyrus main.cpp)
target_compile_options(cyrus PRIVATE "${CYRUS_DEFAULT_COMPILE_OPTIONS}")
target_link_libraries(cyrus PRIVATE cyrus_objects)
