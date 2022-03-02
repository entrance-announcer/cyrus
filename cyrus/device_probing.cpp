#include <fmt/format.h>

#include <algorithm>
#include <cyrus/device_probing.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <tl/expected.hpp>

namespace fs = std::filesystem;
namespace rgs = std::ranges;

namespace cyrus {

tl::expected<Mount_points, std::error_code> read_mounting(
    const std::filesystem::path& block_device) {
  static const constexpr char delim = ' ';
  static const constexpr char* mounts_file = "/etc/mtab";

  std::fstream mtab(mounts_file, std::ios_base::in);
  if (!mtab.is_open()) {
    throw std::runtime_error(fmt::format(
        "The file {} does not exists and is required to read device mounts.\n",
        mounts_file));
  }

  // convert provided block device path to canonical path
  std::error_code ec;
  const fs::path canonical_path = fs::canonical(block_device, ec);
  if (ec) {
    return tl::make_unexpected(ec);
  }

  // extract all mount points of block device or its partitions
  Mount_points device_mounts{};
  std::string mtab_line;
  fs::path mounted_device;
  while (std::getline(mtab, mtab_line)) {
    const auto device_end = rgs::find(mtab_line, delim);
    mounted_device.assign(mtab_line.begin(), device_end);

    const auto comparison = canonical_path <=> mounted_device;
    if (comparison != std::strong_ordering::greater) {
      const auto mnt_point_end = std::find(device_end + 1, mtab_line.end(), delim);
      const auto fs_name_end = std::find(mnt_point_end + 1, mtab_line.end(), delim);
      device_mounts.insert({.path = {device_end + 1, mnt_point_end},
                            .fs_name = {mnt_point_end + 1, fs_name_end}});
    }
  }

  return device_mounts;
}

}  // namespace cyrus
