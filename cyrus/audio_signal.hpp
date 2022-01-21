#pragma once

#include <fmt/core.h>

#include <filesystem>
#include <numeric>  // midpoint
#include <sndfile.hh>
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
      err_msg =
          "Hit EOF while loading audio file before all samples were decoded.";
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
template <Libsndfile_sample T,
          typename Alloc = typename std::vector<T>::allocator_type>
class Audio_signal {
 private:
  constexpr static auto mono_chans = 1;
  constexpr static auto stereo_chans = 2;
  SndfileHandle sndfile_handle{};
  std::vector<T, Alloc> signal{};

  void resize_signal_sf(const sf_count_t new_size) {
    this->signal.resize(static_cast<size_type>(new_size));
  }

 public:
  using value_type = T;
  using allocator_type = Alloc;
  using size_type = typename decltype(signal)::size_type;
  using difference_type = typename decltype(signal)::difference_type;
  using iterator = typename decltype(signal)::iterator;
  using const_iterator = typename decltype(signal)::const_iterator;

  explicit Audio_signal() = default;
  Audio_signal(const Audio_signal&) = default;
  Audio_signal& operator=(const Audio_signal&) = default;

  // SndfileHandle provides no move constructor, so it will be copied.
  // Nevertheless, it implements reference semantics on copy. So, although
  // this is not semantically ideal, and a move constructor should be what
  // implements reference semantics, copying will not be heavy. Additionally,
  // the signal, which is where most of the data is, can be moved.
  Audio_signal(Audio_signal&&) noexcept = default;
  Audio_signal& operator=(Audio_signal&&) noexcept = default;

  Audio_error_code load(const std::filesystem::path& audio_file) {
    auto result = Audio_error_code::no_error;

    // open audio file
    this->sndfile_handle = SndfileHandle(audio_file.c_str());
    if (const auto errc = static_cast<Audio_error_code>(sndfile_handle.error());
        errc != Audio_error_code::no_error) {
      return errc;
    } else if (this->sndfile_handle.channels() < mono_chans ||
               this->sndfile_handle.channels() > stereo_chans) {
      return Audio_error_code::unsupported_number_of_channels;
    }

    // decode and load contents
    const auto num_channels = this->sndfile_handle.channels();
    const auto num_frames = this->sndfile_handle.frames();
    const sf_count_t num_items = num_frames * num_channels;
    this->resize_signal_sf(num_items);
    if (const auto num_read =
            this->sndfile_handle.read(this->signal.data(), num_items);
        num_read < num_items) {
      result = Audio_error_code::hit_eof;
      this->resize_signal_sf(num_read);
    }

    // convert any stereo data to mono by averaging channels
    if (this->sndfile_handle.channels() == stereo_chans) {
      auto read_it = this->signal.cbegin();
      auto write_it = this->signal.begin();
      while (read_it < this->signal.end()) {
        const auto left_sample = *read_it;
        const auto right_sample = *(read_it + 1);
        *write_it = std::midpoint(left_sample, right_sample);
        read_it += stereo_chans;
        write_it += mono_chans;
      }
    }
    this->resize_signal_sf(num_frames);

    return result;
  }

  [[nodiscard]] int format() const { return this->sndfile_handle.format(); }

  [[nodiscard]] int sample_rate() const {
    return this->sndfile_handle.samplerate();
  }

  [[nodiscard]] iterator begin() noexcept { return this->signal.begin(); }

  [[nodiscard]] iterator end() noexcept { return this->signal.end(); }

  [[nodiscard]] const_iterator cbegin() const noexcept {
    return this->signal.cbegin();
  }

  [[nodiscard]] const_iterator cend() const noexcept {
    return this->signal.cend();
  }

  // number of samples
  [[nodiscard]] size_type size() const noexcept { return this->signal.size(); }

  // total number of bytes to store size() samples
  [[nodiscard]] size_type data_size() const noexcept {
    return this->size() * sizeof(value_type);
  }
};

}  // namespace cyrus