#pragma once

#include <fmt/core.h>

#include <algorithm>
#include <bit>
#include <cyrus/audio_signal.hpp>
#include <cyrus/cli.hpp>
#include <cyrus/sample_conversions.hpp>
#include <cyrus/try.hpp>
#include <filesystem>
#include <utility>
#include <vector>


namespace cyrus {

namespace detail {

template <Sample From, Sample To>
using Remap = typename Sample_remapper<To, From>::Remap_values;

template <Sample From>
class Enlarger {
 protected:
  const Parsed_arguments& args;

 public:
  explicit Enlarger(const Parsed_arguments& args) : args{args} {}
  virtual std::pair<From, From> enlarge(const Audio_signal<From>&) const noexcept = 0;
  virtual ~Enlarger() noexcept = default;
};

template <Sample From, Sample To>
class Echo_enlarger : public Enlarger<From> {
 private:
  const Remap<From, To>& remap;

 public:
  explicit Echo_enlarger(const Parsed_arguments& args, const Remap<From, To>& remap)
      : Enlarger<From>{args}, remap{remap} {}
  std::pair<From, From> enlarge(const Audio_signal<From>&) const noexcept override {
    return {remap.from_min, remap.from_max};
  }
};

template <Sample From>
class Min_max_enlarger : public Enlarger<From> {
 public:
  explicit Min_max_enlarger(const Parsed_arguments& args) : Enlarger<From>(args) {}
  std::pair<From, From> enlarge(const Audio_signal<From>& a) const noexcept override {
    const auto [from_min, from_max] = std::ranges::minmax(a);
    return {from_min, from_max};
  }
};

}  // namespace detail

template <Sample From, Sample To>
[[nodiscard]] tl::expected<std::vector<std::vector<std::byte>>, std::string>
convert_audio(const Parsed_arguments& args,
              const std::vector<std::pair<std::filesystem::path, Audio_signal<From>>>&
                  loaded_audios) {
  detail::Remap<From, To> remap_values{.to_min = static_cast<To>(args.range_min),
                                       .to_max = static_cast<To>(args.range_max)};
  detail::Min_max_enlarger<From> min_max(args);
  detail::Echo_enlarger<From, To> echo(args, remap_values);
  detail::Enlarger<From>* enlarger{&echo};
  if (args.enlarge) {
    enlarger = &min_max;
  }

  std::vector<std::vector<std::byte>> converted;
  converted.reserve(loaded_audios.size());
  for (const auto& [in_audio_path, loaded_audio] : loaded_audios) {
    const auto resampled =
        TRY(loaded_audio.resampled(args.sample_rate).map_error([](const auto& err) {
          return fmt::format("Failed to resample: {}.", audio_error_message(err));
        }));

    fmt::print("\t✔ resampled {}", in_audio_path);

    const auto [from_min, from_max] = enlarger->enlarge(resampled);
    remap_values.from_min = from_min;
    remap_values.from_max = from_max;
    const auto remapped = resampled.template remapped<To>(remap_values);
    fmt::print("\r\t✔ resampled  ✔ remapped {}\n", in_audio_path);

    auto* start = std::bit_cast<std::byte*>(remapped.data());
    converted.emplace_back(start, start + remapped.size() * sizeof(To));
  }

  return converted;
}

}  // namespace cyrus
