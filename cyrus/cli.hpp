#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace cyrus {

constexpr const int default_bit_depth{12};
constexpr const int default_word_size{2};

struct Parsed_arguments {
  std::filesystem::path block_device{};
  std::vector<std::filesystem::path> audio_files{};
  bool help{false};
  bool append{false};
  int bit_depth{default_bit_depth};
  int word_size{default_word_size};
};

std::string help_message();

Parsed_arguments parse_arguments(int argc, const char* const argv[]);

class Argument_parse_exception : public std::runtime_error {
  explicit Argument_parse_exception(const std::string&);
};

class User_message_exception : public std::runtime_error {
  explicit User_message_exception(const std::string&);
  explicit User_message_exception(const char*);
};

}  // namespace cyrus
