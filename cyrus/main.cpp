#include <sys/stat.h>

#include <cerrno>
#include <cstring>
#include <cyrus/patched_audio_file.hpp>
#include <filesystem>
#include <iostream>
#include <libcyrus/cli.hpp>
#include <libcyrus/device_probing.hpp>

namespace fs = std::filesystem;

int main(const int argc, const char* const argv[]) {
  cyrus::Parsed_arguments args;

  try {
    args = cyrus::parse_arguments(argc, argv);
  } catch (const cyrus::Argument_parse_error& err) {
    std::cerr << err.what() << "-hello-" << std::endl;
    return -1;
  }

  if (args.help) {
    std::cout << cyrus::help_message << std::endl;
    return 0;
  }

  try {
    if (const auto device_mounting = cyrus::read_mounting(args.block_device);
        device_mounting) {
      switch (device_mounting.value()) {
        case cyrus::Block_mounting::device_mounted:
          std::cout << "The block device cannot be mounted." << std::endl;
          return -1;
        case cyrus::Block_mounting::partition_mounted:
          std::cout << "The block device cannot have mounted partitions."
                    << std::endl;
          return -1;
        default:;
      }
    }
  } catch (const fs::filesystem_error& err) {
    std::cout << err.what() << std::endl;
    return -1;
  }

  struct stat device_status {};
  if (const auto err = stat(args.block_device.c_str(), &device_status); err) {
    std::cerr << std::strerror(errno) << std::endl;
    return -1;
  }

  if (!cyrus::is_block_device(device_status)) {
    std::cerr << std::strerror(errno) << std::endl;
    return -1;
  }

  if (cyrus::is_disk_partition(device_status)) {
    std::cerr << args.block_device
              << " is a disk partition, not the root block device. Please "
                 "provide the root block device."
              << std::endl;
    return -1;
  }

  if (const auto [size, err] = cyrus::total_size(args.block_device); err) {
    std::cerr << err.message() << std::endl;
    return -1;
  } else {
    std::cout << "remaining size: " << size << std::endl;
  }

  std::cout << fs::current_path() << std::endl;
  for (const auto& audio_file_path : args.audio_files) {
    // wave file exists
    try {
      if (!fs::exists(audio_file_path)) {
        std::cerr << "The audio file " << audio_file_path << " doesn't exist."
                  << std::endl;
        return -1;
      }
    } catch (const fs::filesystem_error& err) {
      std::cerr << err.what() << std::endl;
      return -1;
    }

    AudioFile<float> audio_file;
    const bool loaded = audio_file.load(audio_file_path);
    if (!loaded) {
      std::cerr << "An error occurred while decoding " << audio_file_path << '.'
                << std::endl;
      return -1;
    }
    audio_file.printSummary();
  }

  // convert all files to points of our desired sample rate
  // compute total size of all data points
  // check that the disk can store all data points
}
