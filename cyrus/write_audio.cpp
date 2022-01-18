#include <fmt/core.h>
#include <fmt/ostream.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstring>  // strerror, std::size_t
#include <cyrus/device_probing.hpp>
#include <cyrus/patched_audio_file.hpp>
#include <cyrus/user_message_exception.hpp>
#include <cyrus/write_audio.hpp>
#include <filesystem>
#include <type_traits>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using Audio_file = AudioFile<float>;

namespace cyrus {

namespace {

using Block_device_path =
    std::remove_cvref_t<decltype(Parsed_arguments::block_device)>;

// throws instead of returning error - this is part of the cli's interaction,
// so that's what we want
void throw_invalid_block_device(const Block_device_path& block_device) {
  // get device status
  struct stat device_status {};
  if (const auto err = stat(block_device.c_str(), &device_status); err) {
    // failed obtaining device status. Still show as error msg to user, as
    // this is commonly a consequence of improper usage, such as invalid user
    // permissions
    throw User_message_exception(
        fmt::format("{}: {}", block_device, std::strerror(errno)));
  }

  if (!cyrus::is_block_device(device_status)) {
    throw User_message_exception(
        fmt::format("{}: {}", block_device, std::strerror(errno)));
  }

  if (cyrus::is_disk_partition(device_status)) {
    throw User_message_exception(
        fmt::format("{} is a disk partition, not the root block device. Please "
                    "provide the root block device.",
                    block_device));
  }

  // read device's mounting
  try {
    if (const auto device_mounting = cyrus::read_mounting(block_device);
        device_mounting) {
      throw User_message_exception(
          device_mounting.value() == cyrus::Block_mounting::partition_mounted
              ? "The block device must not have mounted partitions."
              : "The block device must not be mounted.");
    }
  } catch (const fs::filesystem_error& err) {
    // failed reading device mounting. Still show as error msg to user, as
    // this is commonly a consequence of improper usage, such as providing
    // a non-existant device path
    throw User_message_exception(err.what());
  }
}

using Audio_file_paths =
    std::remove_cvref_t<decltype(Parsed_arguments::audio_files)>;

[[nodiscard]] std::vector<Audio_file> load_audio_files(
    const Audio_file_paths& audio_file_paths) {
  std::vector<Audio_file> audio_files;
  audio_files.reserve(audio_file_paths.size());

  for (const auto& audio_file_path : audio_file_paths) {
    try {
      if (!fs::exists(audio_file_path)) {
        throw User_message_exception(
            fmt::format("The audio file {} doesn't exist.", audio_file_path));
      }
    } catch (const fs::filesystem_error& err) {
      throw User_message_exception(err.what());
    }

    Audio_file audio_file;
    if (const bool loaded = audio_file.load(audio_file_path); !loaded) {
      throw User_message_exception(
          fmt::format("An error occurred while loading {}\n", audio_file_path));
    }
    audio_file.printSummary();
    audio_files.push_back(std::move(audio_file));
  }

  return audio_files;
}

// TODO: clarify these, put somewhere else, configurable
// all for out block device
constexpr int bit_depth = 12;         // bits
constexpr int word_size = 16;         // bits
constexpr int sample_rate = 500'000;  // samples/sec
constexpr double sample_period = 1 / sample_rate;

}  // namespace

void write_audio_to_device(const Parsed_arguments& args) {
  throw_invalid_block_device(args.block_device);
  fmt::print("Verified block device {}\n", args.block_device);

  // load all audio files before writing, to ensure they can all be
  // first opened loaded without decoding issues.
  const auto loaded_audio_files = load_audio_files(args.audio_files);
  fmt::print("Loaded all input files.\n");

  // length * sample_rate * sample_size * num_audio_files
  double total_length_sec = 0;
  for (const auto& loaded_audio : loaded_audio_files) {
    total_length_sec += loaded_audio.getLengthInSeconds();
  }

  // TODO: get a more accurate measurement, including any headers and
  //  filesystem meta
  auto total_write_size =
      static_cast<std::size_t>(total_length_sec * sample_rate * word_size *
                               static_cast<float>(loaded_audio_files.size()));

  const auto [device_size, err] = cyrus::total_size(args.block_device);
  if (err) {
    throw User_message_exception(err.message());
  }

  if (device_size < total_write_size * 2) {
    throw User_message_exception(
        "Cannot write the audio files to the "
        "provided block device, as it isn't large enough.");
  }

  // TODO: write converted data points to device
}

}  // namespace cyrus
