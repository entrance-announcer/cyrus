#pragma once

#include <fmt/core.h>

#include <bit>
#include <concepts>
#include <limits>
#include <ranges>
#include <source_location>
#include <stdexcept>

namespace cyrus {

using Ratio_t = double;

template <typename SampleTo, typename SampleFrom>
SampleTo convert_sample_type(const SampleFrom from) {
  if constexpr (std::is_same_v<SampleFrom, SampleTo>) {
    return from;
  }
  const Ratio_t from_ratio =
      static_cast<Ratio_t>(from) / std::numeric_limits<SampleFrom>::max();
  const auto converted_sample =
      static_cast<SampleTo>(from_ratio * static_cast<Ratio_t>(from));
  return converted_sample;
}

template <typename Sample>
class Sample_remapper {
 private:
  Ratio_t scale{1};
  Sample shift{0};

 public:
  struct Remap_values {
    Sample from_min{0};
    Sample from_max{0};
    Sample to_min{0};
    Sample to_max{0};
  };
  explicit Sample_remapper(
      const Remap_values& vals,
      const std::source_location loc = std::source_location::current()) {
    const auto from_range = vals.from_max - vals.from_min;
    if (from_range == 0) {
      throw std::domain_error(
          fmt::format("Cannot create a sample remapper for samples "
                      "without a range of possible values. Range: ({} -> {})\n"
                      "Location: {} - ({}:{})\n",
                      vals.from_min, vals.from_max, loc.file_name(), loc.line(),
                      loc.column()));
    }
    this->scale = (vals.to_max - vals.to_min) / from_range;
    this->shift = vals.to_min - vals.from_min * scale;
  }

  inline Sample operator()(const Sample from) const noexcept {
    return scale * from + shift;
  }
};

template <std::integral Sample>
inline Sample flip_sample_endianness(const Sample from) noexcept {
  Sample to;
  const Sample byte_mask = ~0b11111111;
  const int num_bytes_to_swap = sizeof(Sample) / 2 - 1;

  for (int lower_byte_idx = 0; lower_byte_idx < num_bytes_to_swap;
       ++lower_byte_idx) {
    const int upper_byte_idx = sizeof(Sample) - 1 - lower_byte_idx;
    const Sample upper_byte_mask = byte_mask << upper_byte_idx;
    const Sample lower_byte_mask = byte_mask << lower_byte_idx;
    const Sample from_upper_byte = (from & upper_byte_mask) >> upper_byte_idx;
    const Sample from_lower_byte = (from & lower_byte_mask) >> lower_byte_idx;
    to &= from_upper_byte << lower_byte_idx;
    to &= from_lower_byte << upper_byte_idx;
  }
}

}  // namespace cyrus