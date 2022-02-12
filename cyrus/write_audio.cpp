#include <fmt/core.h>
#include <fmt/ostream.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstring>  // strerror, std::size_t
#include <cyrus/audio_signal.hpp>
#include <cyrus/cli.hpp>
#include <cyrus/device_probing.hpp>
#include <cyrus/write_audio.hpp>
#include <filesystem>
#include <type_traits>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace cyrus {

namespace {

using Audio_signal_t = Audio_signal<float>;

using Block_device_path =
    std::remove_cvref_t<decltype(Parsed_arguments::block_device)>;

tl::expected<void, std::string> validate_block_device(
    const Block_device_path& block_device) {
  // get device status
  struct stat device_status {};
  if (const auto err = stat(block_device.c_str(), &device_status); err) {
    // failed obtaining device status. Still show as error msg to user, as
    // this is commonly a consequence of improper usage, such as invalid user
    // permissions
    return tl::make_unexpected(
        fmt::format("{}: {}", block_device, std::strerror(errno)));
  }

  if (!cyrus::is_block_device(device_status)) {
    return tl::make_unexpected(
        fmt::format("{}: {}", block_device, std::strerror(errno)));
  }

  if (cyrus::is_disk_partition(device_status)) {
    return tl::make_unexpected(
        fmt::format("{} is a disk partition, not the root block device. Please "
                    "provide the root block device.",
                    block_device));
  }

  // read device's mounting
  if (const auto device_mounting = cyrus::read_mounting(block_device);
      device_mounting) {
    if (device_mounting != Block_mounting::not_mounted) {
      return tl::make_unexpected(
          device_mounting.value() == cyrus::Block_mounting::partition_mounted
              ? "The block device must not have mounted partitions."
              : "The block device must not be mounted.");
    }
  } else {
    return tl::make_unexpected(device_mounting.error().message());
  }

  return {};
}

using Audio_file_paths =
    std::remove_cvref_t<decltype(Parsed_arguments::audio_files)>;

[[nodiscard]] tl::expected<std::vector<Audio_signal_t>, std::string>
load_audio_files(const Audio_file_paths& audio_file_paths) {
  std::vector<Audio_signal_t> audio_signals;
  audio_signals.reserve(audio_file_paths.size());

  for (const auto& audio_file_path : audio_file_paths) {
    if (!fs::exists(audio_file_path)) {
      tl::make_unexpected(
          fmt::format("The audio file {} doesn't exist.", audio_file_path));
    }

    Audio_signal_t audio_signal;
    if (const auto errc = audio_signal.load(audio_file_path);
        errc == Audio_error_code::hit_eof) {
      fmt::print("Warning: {}: {}\n", audio_error_message(errc),
                 audio_file_path);
    } else if (errc != Audio_error_code::no_error) {
      return tl::make_unexpected(
          fmt::format("An error occurred while loading {}: {}\n",
                      audio_file_path, audio_error_message(errc)));
    }

    fmt::print("audio size: {}\n", audio_signal.size());
    audio_signals.push_back(std::move(audio_signal));
  }

  return audio_signals;
}

}  // namespace

tl::expected<void, std::string> write_audio_to_device(
    const Parsed_arguments& args) {
  if (auto v = validate_block_device(args.block_device); !v) {
    return v;
  }
  fmt::print("Verified block device {}\n", args.block_device);

  // load all audio files before writing, to ensure they can all be
  // first opened loaded without decoding issues.
  const auto loaded = load_audio_files(args.audio_files);
  if (!loaded) {
    return tl::make_unexpected(loaded.error());
  }
  fmt::print("Loaded all input files.\n");
  const auto& loaded_audio_files = loaded.value();

  // length * sample_rate * sample_size * num_audio_files
  double total_length_sec = 0;

  // TODO: get a more accurate measurement, including any headers and
  //  filesystem meta
  auto total_write_size = static_cast<std::size_t>(
      total_length_sec * args.sample_rate * args.word_size *
      static_cast<float>(loaded_audio_files.size()));

  if (const auto dev_size = cyrus::total_size(args.block_device);
      dev_size.value() < total_write_size * 2) {
    return tl::make_unexpected(
        "Cannot write the audio files to the provided block "
        "device, as it isn't large enough.");
  }

  // TODO: write converted data points to device
  return {};
}

}  // namespace cyrus
