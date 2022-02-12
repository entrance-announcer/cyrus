#include <fcntl.h>     // open
#include <linux/fs.h>  // BLKGETSIZE64
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>  // minor
#include <unistd.h>         // close

#include <cerrno>
#include <cyrus/device_probing.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <system_error>
#include <tl/expected.hpp>

namespace fs = std::filesystem;

namespace cyrus {

tl::expected<Block_mounting, std::error_code> read_mounting(
    const fs::path& block_device) {
  std::fstream mtab("/proc/mounts", std::ios_base::in);
  std::error_code ec;
  const fs::path canonical_path = fs::canonical(block_device, ec);
  if (ec) {
    return tl::make_unexpected(ec);
  }

  std::string mtab_line;
  fs::path mounted_device;
  while (std::getline(mtab, mtab_line)) {
    const auto delim_idx = static_cast<std::ptrdiff_t>(mtab_line.find(' '));
    mounted_device.assign(mtab_line.cbegin(), mtab_line.cbegin() + delim_idx);

    const auto comparison = canonical_path <=> mounted_device;
    if (comparison == std::strong_ordering::equal) {
      return Block_mounting::device_mounted;
    } else if (comparison == std::strong_ordering::less) {
      return Block_mounting::partition_mounted;
    }
  }

  return Block_mounting::not_mounted;
}

bool is_block_device(const struct ::stat& device_status) noexcept {
  const auto mode = device_status.st_mode;
  return S_ISBLK(mode);
}

bool is_disk_partition(const struct ::stat& device_status) noexcept {
  const auto device_id = device_status.st_rdev;
  const auto partition_num = minor(device_id);
  return partition_num != 0;
}

tl::expected<std::size_t, std::error_code> total_size(
    const fs::path& block_device) noexcept {
  using Unexpected = tl::unexpected<std::error_code>;
  const int fd = open(block_device.c_str(), O_RDONLY);
  if (fd == -1) {
    return tl::make_unexpected(std::error_code{errno, std::generic_category()});
  }

  std::size_t size;
  if (ioctl(fd, BLKGETSIZE64, &size) != 0) {
    return Unexpected{{errno, std::generic_category()}};  // errno set by open
  }

  if (close(fd) != 0) {
    return Unexpected{{errno, std::generic_category()}};  // errno set by close
  }

  return size;
}

}  // namespace cyrus