#pragma once

#include <fmt/core.h>
#include <samplerate.h>

#include <cmath>
#include <concepts>
#include <cyrus/sample_conversions.hpp>
#include <filesystem>
#include <numeric>  // midpoint
#include <sndfile.hh>
#include <tl/expected.hpp>
#include <vector>

namespace cyrus {

enum class Audio_error_code : int {
  // covers error codes from libsndfile
  no_error,
  unrecognized_format,
  system_error,
  malformed_file,
  unsupported_encoding,
  // additional to those from libsndfile
  unsupported_number_of_channels,
  hit_eof
};

const char* audio_error_message(const Audio_error_code errc) {
  const char* err_msg;
  switch (errc) {
    case Audio_error_code::unsupported_number_of_channels:
      err_msg =
          "Unsupported number of channels. Audio file must have 1 or 2 "
          "channels.";
      break;
    case Audio_error_code::hit_eof:
      err_msg = "Hit EOF while loading audio file before all samples were decoded.";
      break;
    default:
      err_msg = sf_error_number(static_cast<int>(errc));
  }

  return err_msg;
}

template <typename T, typename... Args>
concept One_of = (std::same_as<T, Args> || ...);

template <typename T>
concept Libsndfile_sample = One_of<T, short, int, float, double>;

// represents a single channel audio file, that is loaded from a single
// (mono) or dual (stereo) channel audio file.
template <Sample T, typename Alloc = typename std::vector<T>::allocator_type>
class Audio_signal {
 private:
  constexpr static auto mono_chans = 1;
  constexpr static auto stereo_chans = 2;
  int _sample_rate{0};
  std::vector<T, Alloc> _signal{};

  void resize_signal_sf(const sf_count_t new_size) {
    _signal.resize(static_cast<size_type>(new_size));
  }

 public:
  using value_type = T;
  using allocator_type = Alloc;
  using size_type = typename decltype(_signal)::size_type;
  using difference_type = typename decltype(_signal)::difference_type;
  using iterator = typename decltype(_signal)::iterator;
  using const_iterator = typename decltype(_signal)::const_iterator;

  explicit Audio_signal() = default;
  Audio_signal(const Audio_signal&) = default;
  Audio_signal& operator=(const Audio_signal&) = default;

  // SndfileHandle provides no move constructor, so it will be copied.
  // Nevertheless, it implements reference semantics on copy. So, although
  // this is not semantically ideal, and a move constructor should be what
  // implements reference semantics, copying will not be heavy. Additionally,
  // the _signal, which is where most of the data is, can be moved.
  Audio_signal(Audio_signal&&) noexcept = default;
  Audio_signal& operator=(Audio_signal&&) noexcept = default;

  Audio_error_code load(const std::filesystem::path& audio_file) {
    static_assert(Libsndfile_sample<T>,
                  "To load audio, sample data must be compatible with libsndfile.");
    auto result = Audio_error_code::no_error;

    // open audio file
    SndfileHandle sndfile_handle(audio_file.c_str());
    if (const auto errc = static_cast<Audio_error_code>(sndfile_handle.error());
        errc != Audio_error_code::no_error) {
      return errc;
    } else if (sndfile_handle.channels() < mono_chans ||
               sndfile_handle.channels() > stereo_chans) {
      return Audio_error_code::unsupported_number_of_channels;
    }

    // decode and load contents
    _sample_rate = sndfile_handle.samplerate();
    const sf_count_t num_items = sndfile_handle.channels() * sndfile_handle.frames();
    this->resize_signal_sf(num_items);
    if (const auto num_read = sndfile_handle.read(_signal.data(), num_items);
        num_read < num_items) {
      result = Audio_error_code::hit_eof;
      this->resize_signal_sf(num_read);
    }

    // convert any stereo data to mono by averaging channels
    if (sndfile_handle.channels() == stereo_chans) {
      auto read_it = _signal.cbegin();
      auto write_it = _signal.begin();
      while (read_it < _signal.end()) {
        const auto left_sample = *read_it;
        const auto right_sample = *(read_it + 1);
        *write_it = std::midpoint(left_sample, right_sample);
        read_it += stereo_chans;
        write_it += mono_chans;
      }
    }
    this->resize_signal_sf(sndfile_handle.frames());

    return result;
  }


  template <Sample U>
  Audio_signal<U> remapped(
      const typename Sample_remapper<U, T>::Remap_values& remap_vals = {}) const {
    const Sample_remapper<U, T> remapper(remap_vals);
    Audio_signal<U> remapped;
    remapped.resize(_signal.size());

    for (size_type i = 0; i < _signal.size(); ++i) {
      remapped[i] = remapper(_signal[i]);
    }
    return remapped;
  }


  tl::expected<Audio_signal, Audio_error_code> resampled(const int sample_rate) const {
    static_assert(std::same_as<T, float>,
                  "To resample audio signals, both the input "
                  "and output samples must be stored as floats.");

    if (_sample_rate == sample_rate) {
      return *this;
    }

    Audio_signal resampled_signal;
    resampled_signal._sample_rate = sample_rate;
    const auto resample_ratio{static_cast<double>(sample_rate) / _sample_rate};
    const auto resampled_size{static_cast<sf_count_t>(
        std::ceil(resample_ratio * static_cast<double>(_signal.size())))};
    resampled_signal.resize_signal_sf(resampled_size);

    SRC_DATA conversion_data;
    conversion_data.data_in = _signal.data();
    conversion_data.data_out = resampled_signal._signal.data();
    conversion_data.input_frames = static_cast<long>(_signal.size());
    conversion_data.output_frames = resampled_size;
    conversion_data.src_ratio = resample_ratio;

    const auto src_errc = src_simple(&conversion_data, SRC_SINC_BEST_QUALITY, 1);
    if (const auto errc = static_cast<Audio_error_code>(src_errc);
        errc != Audio_error_code::no_error) {
      return tl::make_unexpected(errc);
    }

    resampled_signal.resize_signal_sf(conversion_data.output_frames_gen);
    return resampled_signal;
  }

  void resize(const size_type size) { _signal.resize(size); }

  [[nodiscard]] int sample_rate() const noexcept { return _sample_rate; }

  [[nodiscard]] value_type& operator[](const size_type idx) noexcept {
    return _signal[idx];
  }

  [[nodiscard]] const value_type& operator[](const size_type idx) const noexcept {
    return _signal[idx];
  }

  [[nodiscard]] iterator begin() noexcept { return _signal.begin(); }

  [[nodiscard]] iterator end() noexcept { return _signal.end(); }

  [[nodiscard]] const_iterator begin() const noexcept { return _signal.begin(); }

  [[nodiscard]] const_iterator end() const noexcept { return _signal.end(); }

  [[nodiscard]] const_iterator cbegin() const noexcept { return _signal.cbegin(); }

  [[nodiscard]] const_iterator cend() const noexcept { return _signal.cend(); }

  // number of samples
  [[nodiscard]] size_type size() const noexcept { return _signal.size(); }

  [[nodiscard]] const char* data() const noexcept {
    return std::bit_cast<const char*>(_signal.data());
  }
};

}  // namespace cyrus
