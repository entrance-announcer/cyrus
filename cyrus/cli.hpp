#pragma once

#include <cyrus/cyrus_main.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <tl/expected.hpp>
#include <vector>

namespace cyrus {

constexpr const int default_word_size{2};
constexpr const int default_sample_rate{40000};
constexpr const uint64_t default_range_max{3890};
constexpr const uint64_t default_range_min{205};

struct Parsed_arguments {
  std::filesystem::path block_device{};
  std::vector<std::filesystem::path> audio_files{};
  bool help{false};
  int word_size{default_word_size};
  uint64_t range_max{default_range_max};
  uint64_t range_min{default_range_min};
  int sample_rate{default_sample_rate};
  bool enlarge{false};
};

std::string help_message();

[[nodiscard]] tl::expected<Parsed_arguments, std::string> parse_arguments(
    Program_arguments prog_args);

[[nodiscard]] bool user_accept_dialog(std::string_view,
                                      std::string_view accept_option = "y",
                                      std::string_view decline_option = "n");

}  // namespace cyrus
