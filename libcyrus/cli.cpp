#include <cstddef>
#include <cstring>
#include <libcyrus/cli.hpp>

namespace cyrus {

namespace {

bool is_help_flag(const char* const arg) noexcept {
  return std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0;
}

bool is_append_flag(const char* const arg) noexcept {
  return std::strcmp(arg, "-a") == 0 || std::strcmp(arg, "--append") == 0;
}

}  // namespace

Argument_parse_error::Argument_parse_error(const std::string& msg)
    : std::runtime_error(msg) {}

Parsed_arguments parse_arguments(const int argc, const char* const argv[]) {
  Parsed_arguments parsed_args{};
  std::ptrdiff_t arg_idx = 1;

  // parse options
  while (arg_idx < argc && argv[arg_idx][0] == '-') {
    if (is_help_flag(argv[arg_idx])) {
      parsed_args.help = true;
      return parsed_args;
    } else if (is_append_flag(argv[arg_idx])) {
      parsed_args.append = true;
    } else {
      throw Argument_parse_error("Unrecognized optional argument provided: " +
                                 std::string(argv[arg_idx]));
    }
    ++arg_idx;
  }

  // parse block device
  if (arg_idx < argc) {
    parsed_args.block_device = argv[arg_idx];
    ++arg_idx;
  } else {
    throw Argument_parse_error(
        "A positional argument naming the block device to write to must be "
        "provided.");
  }

  // parse input wav files
  if (arg_idx < argc) {
    parsed_args.audio_files.reserve(argc - arg_idx - 1);
    while (arg_idx < argc) {
      parsed_args.audio_files.emplace_back(argv[arg_idx]);
      ++arg_idx;
    }
  } else {
    throw Argument_parse_error("At least one input wav file must be provided.");
  }

  return parsed_args;
}

}  // namespace cyrus
