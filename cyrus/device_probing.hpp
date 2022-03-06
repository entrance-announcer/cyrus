#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <tl/expected.hpp>
#include <vector>

namespace cyrus {

struct Mounting {
  std::filesystem::path mount_point{};
  std::string fs_name{};
};

// device -> mounting
using Mountings = std::map<std::filesystem::path, Mounting>;

[[nodiscard]] tl::expected<Mountings, std::error_code> read_mounting(
    const std::filesystem::path&);

// reads all block devices that are part of the same physical drive as that provided
[[nodiscard]] std::vector<std::filesystem::path> read_drive_partitions(
    const std::filesystem::path&);

}  // namespace cyrus
