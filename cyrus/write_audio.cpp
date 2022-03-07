#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <cyrus/audio_signal.hpp>
#include <cyrus/cli.hpp>
#include <cyrus/device_probing.hpp>
#include <cyrus/sample_conversions.hpp>
#include <cyrus/try.hpp>
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
      return tl::make_unexpected(
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

    audio_signals.emplace_back(audio_file_path, std::move(audio_signal));
    fmt::print("\t✔ loaded {}\n", audio_file_path);
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
        "from partition 1 of its storage device.");
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

[[nodiscard]] tl::expected<std::vector<Audio_signal_t>, std::string> resample_audio(
    const Parsed_arguments& args,
    const std::vector<std::pair<fs::path, Audio_signal_t>>& loaded_audios) {
  std::vector<Audio_signal_t> resampled_audios;
  resampled_audios.reserve(loaded_audios.size());
  for (const auto& [in_audio_path, loaded_audio] : loaded_audios) {
    auto&& resampled = loaded_audio.resample(args.sample_rate);
    if (!resampled) {
      return tl::make_unexpected(fmt::format("Failed to resample {}: {}.", in_audio_path,
                                             audio_error_message(resampled.error())));
    }
    resampled_audios.push_back(resampled.value());
    fmt::print("\t✔ resampled {}\n", in_audio_path);
  }

  return resampled_audios;
}

}  // namespace

tl::expected<void, std::string> write_audio_to_device(const Parsed_arguments& args) {
  fmt::print("Verifying block device {}... ", args.block_device);
  REQ(block_device_validity_checks(args.block_device))

  const auto mounting = TRY(get_destination_mounting(args.block_device));

  // check that filesystem of provided path is FAT
  if (mounting.fs_name != "vfat") {
    return tl::make_unexpected(
        fmt::format("The block device, {}, is incorrectly formatted with the {} "
                    "filesystem. It must be formatted in the FAT32 filesystem.",
                    args.block_device, mounting.fs_name));
  }
  fmt::print("✔\n");

  // load all audio files before writing, to ensure they can all be
  // first opened loaded without decoding issues.
  fmt::print("Loading audio files... \n");
  const auto loaded_audios = TRY(load_audio_files(args.audio_files));

  // resample audio
  fmt::print("Resampling audio to {} samples/s... \n", args.sample_rate);
  const auto resampled_audios = TRY(resample_audio(args, loaded_audios));


  // ensure that specified device has sufficient available space
  const auto write_samples = std::accumulate(
      resampled_audios.cbegin(), resampled_audios.cend(), 0u,
      [](const auto& acc, const auto& curr) { return acc + curr.size(); });

  if (const auto available_space = fs::space(mounting.mount_point).available;
      available_space < write_samples * static_cast<std::size_t>(args.word_size)) {
    return tl::make_unexpected(
        "The provided block device lacks the available space to "
        "store the specified audio files");
  }

  // prompt user before writing
  if (!user_accept_dialog(
          fmt::format("\nWould you like to proceed to write {} raw audio file{}onto {}?",
                      resampled_audios.size(), resampled_audios.empty() ? " " : "s ",
                      mounting.mount_point))) {
    return {};
  }

  // write resampled audio to block device
  for (std::size_t audio_idx = 0; audio_idx < resampled_audios.size(); ++audio_idx) {
    const auto& in_audio_path = loaded_audios[audio_idx].first;
    const auto& resampled = resampled_audios[audio_idx];
    const auto out_path =
        mounting.mount_point / in_audio_path.filename().replace_extension("raw");
    const auto open_mode =
        std::ios_base::out | std::ios_base::binary | std::ios_base::trunc;

    std::ofstream out_file(out_path.string(), open_mode);
    if (!out_file.good()) {
      return tl::make_unexpected(
          fmt::format("Couldn't open the destination file: {}.", out_path));
    }

    // TODO: convert resampled audio's samples to appropriate raw format
    out_file.write(resampled.data(), static_cast<std::streamsize>(resampled.data_size()));
  }

  return {};
}

}  // namespace cyrus
