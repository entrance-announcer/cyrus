#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstring>  // strerror, std::size_t
#include <cyrus/audio_signal.hpp>
#include <cyrus/cli.hpp>
#include <cyrus/device_probing.hpp>
#include <cyrus/write_audio.hpp>
#include <filesystem>
#include <ranges>
#include <tl/expected.hpp>
#include <type_traits>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace rgs = std::ranges;
namespace vws = rgs::views;

namespace cyrus {

namespace {

using Audio_signal_t = Audio_signal<float>;

using Audio_file_paths = std::remove_cvref_t<decltype(Parsed_arguments::audio_files)>;

[[nodiscard]] tl::expected<std::vector<Audio_signal_t>, std::string> load_audio_files(
    const Audio_file_paths& audio_file_paths) {
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
      fmt::print("Warning: {}: {}\n", audio_error_message(errc), audio_file_path);
    } else if (errc != Audio_error_code::no_error) {
      return tl::make_unexpected(fmt::format("An error occurred while loading {}: {}\n",
                                             audio_file_path, audio_error_message(errc)));
    }

    fmt::print("audio size: {}\n", audio_signal.size());
    audio_signals.push_back(std::move(audio_signal));
  }

  return audio_signals;
}

}  // namespace

tl::expected<void, std::string> write_audio_to_device(const Parsed_arguments& args) {
  // check if device exists
  if (!fs::exists(args.block_device)) {
    return tl::make_unexpected(
        fmt::format("The block device {} does not exist.\n", args.block_device));
  }

  // check if it's a block device
  if (!fs::is_block_file(args.block_device)) {
    return tl::make_unexpected(
        fmt::format("The file {} is not a block file.\n", args.block_device));
  }

  // get mount points of the provided device and any of its partitions
  const auto mount_points = read_mounting(args.block_device);
  if (!mount_points) {
    return tl::make_unexpected(mount_points.error().message());
  }

  // check if the provided device has partitions
  // if it has partitions that aren't what was provided -> error
  if (mount_points.value().size() == 0) {
    return tl::make_unexpected(
        fmt::format("The device {} is not mounted.\n", args.block_device));

  } else if (mount_points.value().size() > 1) {
    const auto mount_paths =
        mount_points.value() | vws::transform([](const auto& m) { return m.path; });

    return tl::make_unexpected(
        fmt::format("The device {} has multiple mounted partitions. This will "
                    "not be readable by Miley. Mount points: {}\n",
                    args.block_device, mount_paths));
  }

  // get mount point of provided path
  const auto mount_point = *mount_points.value().cbegin();

  // check that filesystem of provided path is FAT
  if (mount_point.fs_name != "vfat") {
    return tl::make_unexpected(
        fmt::format("The block device, {}, is incorrectly formatted with the {} "
                    "filesystem. It must be formatted in the FAT32 filesystem.",
                    args.block_device, mount_point.fs_name));
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
  auto total_write_size =
      static_cast<std::size_t>(total_length_sec * args.sample_rate * args.word_size *
                               static_cast<float>(loaded_audio_files.size()));

  if (const auto dev_size = 1E7; dev_size < total_write_size * 2) {
    return tl::make_unexpected(
        "Cannot write the audio files to the provided block "
        "device, as it isn't large enough.");
  }

  // TODO: write converted data points to device
  return {};
}

}  // namespace cyrus
