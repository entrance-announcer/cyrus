#pragma once

#include <fmt/format.h>

#include <bit>
#include <concepts>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace cyrus {

using Ratio_t = double;

template <typename T>
concept Sample = (std::integral<T> || std::floating_point<T>);

template <typename SampleFrom, typename SampleTo>
concept ConvertibleSample = Sample<SampleTo> && Sample<SampleFrom> &&
    std::convertible_to<SampleFrom, SampleTo>;

template <Sample SampleTo, ConvertibleSample<SampleTo> SampleFrom>
requires std::convertible_to<SampleFrom, Ratio_t> SampleTo
convert_sample_type(const SampleFrom from) {
  if constexpr (std::is_same_v<SampleFrom, SampleTo>) {
    return from;
  }

  const Ratio_t from_val = from;
  const auto from_type_max{std::numeric_limits<SampleFrom>::max()};
  const Ratio_t from_ratio{from_val / from_type_max};

  return from_ratio * from_val;
}

template <Sample SampleTo, ConvertibleSample<SampleTo> SampleFrom>
requires std::convertible_to<SampleFrom, Ratio_t>
class Sample_remapper {
 private:
  Ratio_t scale{1.0};
  SampleTo shift{0};

 public:
  struct Remap_values {
    SampleFrom from_min{0};
    SampleFrom from_max{0};
    SampleTo to_min{0};
    SampleTo to_max{0};
  };

  explicit Sample_remapper(
      const Remap_values &vals,
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
    this->shift = vals.to_min - vals.from_min * this->scale;
  }

  inline SampleTo operator()(const SampleFrom from) const noexcept {
    return static_cast<SampleTo>(this->scale * from) + this->shift;
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