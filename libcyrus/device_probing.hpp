#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <utility>

namespace cyrus {

enum class Block_mounting { device_mounted, partition_mounted };

std::optional<Block_mounting> read_mounting(
    const std::filesystem::path& block_device);

bool is_block_device(const struct stat& device_status) noexcept;

bool is_disk_partition(const struct stat& device_status) noexcept;

std::pair<std::size_t, std::error_code> total_size(
    const std::filesystem::path& block_device) noexcept;

}  // namespace cyrus