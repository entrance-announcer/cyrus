#pragma once

#include <cstddef>
#include <filesystem>
#include <set>
#include <tl/expected.hpp>

namespace cyrus {

struct Mount_point {
  std::filesystem::path path{};
  std::string fs_name{};
  friend bool operator<(const Mount_point& m1, const Mount_point& m2) {
    return m1.path < m2.path;
  }
};

using Mount_points = std::set<Mount_point>;

[[nodiscard]] tl::expected<Mount_points, std::error_code> read_mounting(
    const std::filesystem::path& block_device);

}  // namespace cyrus
