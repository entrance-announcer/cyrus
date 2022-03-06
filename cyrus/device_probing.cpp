#include <fmt/format.h>

#include <algorithm>
#include <cyrus/device_probing.hpp>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <tl/expected.hpp>
#include <vector>

namespace fs = std::filesystem;
namespace rgs = std::ranges;

namespace cyrus {

namespace {
const constexpr char delim = ' ';
}

tl::expected<Mountings, std::error_code> read_mounting(
    const std::filesystem::path& block_device) {
  static const constexpr char* mounts_path = "/etc/mtab";

  std::fstream mtab(mounts_path, std::ios_base::in);
  if (!mtab.good()) {
    throw fs::filesystem_error(
        fmt::format("The file {} could not be opened and is required to read device "
                    "mounts",
                    mounts_path),
        std::make_error_code(std::errc::no_such_file_or_directory));
  }

  // convert provided block device path to canonical path
  std::error_code ec;
  const fs::path canonical_path = fs::canonical(block_device, ec);
  if (ec) {
    return tl::make_unexpected(ec);
  }

  // extract all mount points of block device or its partitions
  Mountings device_mounts{};
  std::string mtab_line;
  fs::path mounted_device;
  while (std::getline(mtab, mtab_line)) {
    const auto device_end = rgs::find(mtab_line, delim);
    mounted_device.assign(mtab_line.begin(), device_end);

    const auto comparison = canonical_path <=> mounted_device;
    if (comparison != std::strong_ordering::greater) {
      const auto mnt_point_end = std::find(device_end + 1, mtab_line.end(), delim);
      const auto fs_name_end = std::find(mnt_point_end + 1, mtab_line.end(), delim);
      Mounting mounting{.mount_point = {device_end + 1, mnt_point_end},
                        .fs_name = {mnt_point_end + 1, fs_name_end}};
      device_mounts.try_emplace(mounted_device, std::move(mounting));
    }
  }

  return device_mounts;
}

// get the partitions of the physical drive of the provided block device
std::vector<fs::path> read_drive_partitions(const std::filesystem::path& block_device) {
  // Open system's partitions file
  static constexpr const char* const partitions_path{"/proc/partitions"};
  std::fstream partitions(partitions_path, std::ios_base::in);
  if (!partitions.good()) {
    throw fs::filesystem_error(
        fmt::format("The file {} could not be opened and is required to read device "
                    "partitions",
                    partitions_path),
        std::make_error_code(std::errc::no_such_file_or_directory));
  }

  // Store all partitions that belong to same physical drive as block device provided
  std::vector<fs::path> drive_partitions{};
  std::string partition_line;
  fs::path device_partition;
  bool passed_root_partition{false};

  for (int i = 0; i < 2; ++i) {
    // skip header & break line
    std::getline(partitions, partition_line);
  }

  while (std::getline(partitions, partition_line)) {
    // form block device path from listed partition name
    const auto partition_name_start = partition_line.find_last_of(delim) + 1;
    if (partition_name_start == std::string::npos) {
      continue;
    }
    device_partition.assign(
        partition_line.begin() + static_cast<std::ptrdiff_t>(partition_name_start),
        partition_line.end());
    device_partition = "/dev" / device_partition;

    // check if device partition belongs to provided block device
    if (const auto in_same_drive =
            block_device.string().starts_with(device_partition.string());
        !in_same_drive) {
      if (drive_partitions.empty()) {
        // haven't reached pertinent drive yet
        continue;
      } else {
        // already read all of pertinent drive
        break;
      }
    }

    // add all non-root drive partitions
    if (!passed_root_partition) {
      passed_root_partition = true;
    } else {
      drive_partitions.push_back(device_partition);
    }
  }

  return drive_partitions;
}


}  // namespace cyrus
