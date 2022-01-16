#pragma once

#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace cyrus {

struct Parsed_arguments {
  std::filesystem::path block_device{};
  std::vector<std::filesystem::path> audio_files{};
  bool append{false};
  bool help{false};
};

// clang-format off
inline constexpr std::string_view help_message =
    "Usage: cyrus [options] <block_device> <audio_files...>\n"
    "Write the provided wav and aiff files to the block device\n"
    "\n"
    "Ex. cyrus /dev/nvme0n1 ordinary_girl.aiff nobodys_perfect.wav who_said"
".wav\n"
    "\n"
    "Positional Arguments:\n"
    "block_device\tDestination block device\n"
    "audio_files\tInput wav and aiff formatted audio files\n"
    "\n"
    "Optional Arguments\n"
    "-h --help\tshow this help message and exit\n"
    "-a --append\tAppend audio_files to the block device instead of overwriting its contents";
// clang-format on

Parsed_arguments parse_arguments(int argc, const char* const argv[]);

class Argument_parse_error : public std::runtime_error {
 public:
  explicit Argument_parse_error(const std::string&);
};

}  // namespace cyrus
