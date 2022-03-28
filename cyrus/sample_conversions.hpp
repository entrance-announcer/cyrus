#pragma once

#include <fmt/format.h>

#include <bit>
#include <concepts>
#include <limits>
#include <numeric>
#include <source_location>
#include <stdexcept>

namespace cyrus {

using Ratio_t = double;

template <typename T>
concept Sample = (std::integral<T> || std::floating_point<T>);

template <typename SampleFrom, typename SampleTo>
concept ConvertibleSample =
    Sample<SampleTo> && Sample<SampleFrom> && std::convertible_to<SampleFrom, SampleTo>;


static constexpr float norm_float_min{-1.0f};
static constexpr float norm_float_max{+1.0f};

template <Sample SampleTo, ConvertibleSample<SampleTo> SampleFrom>
requires std::convertible_to<SampleFrom, Ratio_t>
class Sample_remapper {
 private:
  Ratio_t scale{1.0};
  Ratio_t shift{0};

 public:
  struct Remap_values {
    SampleFrom from_min{norm_float_min};
    SampleFrom from_max{norm_float_max};
    SampleTo to_min{std::numeric_limits<SampleTo>::min()};
    SampleTo to_max{std::numeric_limits<SampleTo>::max()};
  };

  explicit Sample_remapper(
      const Remap_values &vals,
      const std::source_location loc = std::source_location::current()) {
    const auto to_min = static_cast<Ratio_t>(vals.to_min);
    const auto to_max = static_cast<Ratio_t>(vals.to_max);
    const auto from_min = static_cast<Ratio_t>(vals.from_min);
    const auto from_max = static_cast<Ratio_t>(vals.from_max);

    const auto from_range = vals.from_max - vals.from_min;
    if (from_range == 0) {
      throw std::domain_error(fmt::format(
          "Cannot create a sample remapper for samples "
          "without a range of possible values. Range: ({} -> {})\n"
          "Location: {} - ({}:{})\n",
          vals.from_min, vals.from_max, loc.file_name(), loc.line(), loc.column()));
    }

    this->scale = (to_max - to_min) / from_range;
    this->shift =
        std::abs(std::midpoint(to_min, to_max) - std::midpoint(from_min, from_max));
  }

  inline SampleTo operator()(const SampleFrom from) const noexcept {
    return static_cast<SampleTo>(this->scale * from);
  }
};

// return value will be interpreted completely differently on the running
// architecture
template <Sample S>
inline S flip_sample_endianness(S sample) noexcept {
  auto *upper = static_cast<std::byte *>(&sample);
  auto *lower = upper + sizeof(S) - 1;
  while (upper != lower) {
    std::swap(upper, lower);
    ++upper;
    --lower;
  }
  return sample;
}

}  // namespace cyrus