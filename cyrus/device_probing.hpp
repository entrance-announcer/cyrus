#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <tl/expected.hpp>
#include <utility>

namespace cyrus {

enum class Block_mounting { not_mounted, device_mounted, partition_mounted };

tl::expected<Block_mounting, std::error_code> read_mounting(
    const std::filesystem::path& block_device);

bool is_block_device(const struct stat& device_status) noexcept;

bool is_disk_partition(const struct stat& device_status) noexcept;

tl::expected<std::size_t, std::error_code> total_size(
    const std::filesystem::path& block_device) noexcept;

}  // namespace cyrus