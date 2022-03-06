#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <cyrus/audio_signal.hpp>
#include <cyrus/cli.hpp>
#include <cyrus/device_probing.hpp>
#include <cyrus/write_audio.hpp>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <tl/expected.hpp>
#include <type_traits>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace vws = std::ranges::views;

namespace cyrus {

namespace {

using Audio_signal_t = Audio_signal<float>;
using Audio_file_paths = std::remove_cvref_t<decltype(Parsed_arguments::audio_files)>;

[[nodiscard]] tl::expected<std::vector<std::pair<fs::path, Audio_signal_t>>, std::string>
load_audio_files(const Audio_file_paths& audio_file_paths) {
  std::vector<std::pair<fs::path, Audio_signal_t>> audio_signals;
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
    audio_signals.emplace_back(audio_file_path, std::move(audio_signal));
  }

  return audio_signals;
}

[[nodiscard]] tl::expected<void, std::string> block_device_validity_checks(
    const fs::path& block_device) {
  // check if device exists
  if (!fs::exists(block_device)) {
    return tl::make_unexpected(
        fmt::format("The block device {} does not exist.\n", block_device));
  }

  // check if it's a block device
  if (!fs::is_block_file(block_device)) {
    return tl::make_unexpected(
        fmt::format("The file {} is not a block file.\n", block_device));
  }

  // ensure a partition that's compatible with Miley was specified
  const auto drive_partitions = read_drive_partitions(block_device);
  if (drive_partitions.empty()) {
    return tl::make_unexpected(
        "The specified block device is part of a drive without partitions. Miley reads "
        "from partition 1 of its device.");
  }
  if (block_device != drive_partitions[0]) {
    return tl::make_unexpected(
        "The specified block device must refer to the first partition of its drive");
  }

  return {};
}

[[nodiscard]] tl::expected<Mounting, std::string> get_destination_mounting(
    const fs::path& block_device) {
  // get mount points of the provided device and any of its partitions
  const auto mountings_e = read_mounting(block_device);
  if (!mountings_e) {
    return tl::make_unexpected(mountings_e.error().message());
  }
  const auto& mountings = mountings_e.value();

  // ensure device is mounted
  if (mountings.empty()) {
    return tl::make_unexpected(
        fmt::format("The device {} is not mounted.\n", block_device));
  }

  // get mount point of provided path
  const auto mount_it = mountings.find(block_device);
  if (mount_it == mountings.end()) {
    auto mounted_list = mountings | vws::transform([](const auto& p) { return p.first; });
    return tl::make_unexpected(
        fmt::format("The provided block device {} is mounted but could not be found in "
                    "the system's mounted devices: {}\n",
                    block_device, mounted_list));
  }
  return mount_it->second;
}

}  // namespace

tl::expected<void, std::string> write_audio_to_device(const Parsed_arguments& args) {
  if (const auto dev_valid = block_device_validity_checks(args.block_device);
      !dev_valid) {
    return dev_valid;
  }

  const auto mounting_e = get_destination_mounting(args.block_device);
  if (!mounting_e) {
    return tl::make_unexpected(mounting_e.error());
  }
  const auto& mounting = mounting_e.value();

  // check that filesystem of provided path is FAT
  if (mounting.fs_name != "vfat") {
    return tl::make_unexpected(
        fmt::format("The block device, {}, is incorrectly formatted with the {} "
                    "filesystem. It must be formatted in the FAT32 filesystem.",
                    args.block_device, mounting.fs_name));
  }
  fmt::print("Verified block device {}\n", args.block_device);

  // load all audio files before writing, to ensure they can all be
  // first opened loaded without decoding issues.
  const auto loaded = load_audio_files(args.audio_files);
  if (!loaded) {
    return tl::make_unexpected(loaded.error());
  }
  const auto& loaded_audios = loaded.value();
  fmt::print("Loaded all audio files.\n");

  // length * sample_rate * sample_size * num_audio_files
  double total_length_sec = 0;

  // TODO: get a more accurate measurement, including any headers and
  //  filesystem meta
  auto total_write_size =
      static_cast<std::size_t>(total_length_sec * args.sample_rate * args.word_size *
                               static_cast<float>(loaded_audios.size()));

  if (const auto dev_size = 1E7; dev_size < total_write_size * 2) {
    return tl::make_unexpected(
        "Cannot write the audio files to the provided block "
        "device, as it isn't large enough.");
  }

  // open output files

  for (const auto& [in_audio_path, loaded_audio] : loaded_audios) {
    const auto open_mode = std::ios_base::out | std::ios_base::binary;
    const auto out_path = mounting.mount_point / in_audio_path.stem();
    std::ofstream out_file(out_path.string(), open_mode);
    if (!out_file.good()) {
      return tl::make_unexpected(
          fmt::format("Couldn't open the destination file: {}.", out_path));
    }
    // transform loaded audio to file format we want, maybe with resampling
    out_file << "testing";
    out_file.flush();
  }

  return {};
}

}  // namespace cyrus
