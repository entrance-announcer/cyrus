#pragma once

#include <cyrus/cyrus_main.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <tl/expected.hpp>
#include <vector>

namespace cyrus {

constexpr const int default_bit_depth{12};
constexpr const int default_word_size{2};
constexpr const int default_sample_rate{44000};

struct Parsed_arguments {
  std::filesystem::path block_device{};
  std::vector<std::filesystem::path> audio_files{};
  bool help{false};
  int bit_depth{default_bit_depth};
  int word_size{default_word_size};
  int sample_rate{default_sample_rate};
  bool maximize{false};
};

std::string help_message();

[[nodiscard]] tl::expected<Parsed_arguments, std::string> parse_arguments(
    Program_arguments prog_args);

[[nodiscard]] bool user_accept_dialog(std::string_view,
                                      std::string_view accept_option = "y",
                                      std::string_view decline_option = "n");

}  // namespace cyrus
