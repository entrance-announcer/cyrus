#include <fmt/format.h>

#include <charconv>
#include <cstring>
#include <cyrus/cli.hpp>
#include <string>

namespace cyrus {

namespace {

// clang-format off
constexpr const char* const help_message_fmt =
    "Usage: cyrus [options] <block_device> <audio_files...>\n"
    " Write the provided wav and aiff files to the block device\n"
    "\n"
    "Ex. 1: cyrus /dev/nvme0n1 ordinary_girl.aiff nobodys_perfect.wav who_said.wav\n"
    "Ex. 2: cyrus -b=24 -w=32 /dev/nvme0n1 he_coule_be_the_one.aif\n"
    "\n"
    "Positional Arguments:\n"
    "block_device\tDestination block device\n"
    "audio_files\tInput wav and aiff formatted audio files\n"
    "\n"
    "Optional Arguments:\n"
    "-h --help\tShow this help message and exit\n"
    "-a --append\tAppend audio_files to the block device instead of overwriting its contents.\n"
    "\t\tProhibits the use of -b, -w, and -s flags, for these values are already stored on disk.\n"
    "-b --bit_depth <int>\tNumber of bits to store written samples [Default {}]\n"
    "-w --word_size <int>\tNumber of bytes per written word [Default {}]\n";
// clang-format on

bool is_help_flag(const char* const arg) noexcept {
  return std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0;
}

bool is_append_flag(const char* const arg) noexcept {
  return std::strcmp(arg, "-a") == 0 || std::strcmp(arg, "--append") == 0;
}

bool c_starts_with(const char* const str, const char* const prefix) {
  return strncmp(prefix, str, strlen(prefix)) == 0;
}

int is_bit_depth_flag(const char* const arg) noexcept {
  return c_starts_with(arg, "-b") || c_starts_with(arg, "--bit_depth");
}

int is_word_size_flag(const char* const arg) noexcept {
  return c_starts_with(arg, "-w") || c_starts_with(arg, "--word_size");
}

int next_arg_to_int(const int argc, const char* const argv[],
                    const int flag_arg_idx, const char* const option_name) {
  // read next argument (that following the flag); where the value should be
  const int val_arg_idx = flag_arg_idx + 1;
  if (val_arg_idx >= argc) {
    throw Argument_parse_exception(
        fmt::format("Expected an integer value following the provided {} flag, "
                    "{}.",
                    option_name, argv[flag_arg_idx]));
  }

  // parse the value argument to an int
  int val;
  const char* const val_arg = argv[val_arg_idx];
  if (const auto result =
          std::from_chars(val_arg, val_arg + std::strlen(val_arg), val);
      result.ec != std::errc{}) {
    throw Argument_parse_exception(fmt::format(
        "Failed parsing argument {} to an integer for option {}: {}\n",
        result.ptr, option_name, std::strerror(static_cast<int>(result.ec))));
  }
  return val;
}

}  // namespace

std::string help_message() {
  return fmt::format(help_message_fmt, default_bit_depth, default_word_size);
}

Argument_parse_exception::Argument_parse_exception(const std::string& msg)
    : std::runtime_error(msg) {}

User_message_exception::User_message_exception(const std::string& msg)
    : std::runtime_error{msg} {}

User_message_exception::User_message_exception(const char* const msg)
    : std::runtime_error{msg} {}

Parsed_arguments parse_arguments(const int argc, const char* const argv[]) {
  Parsed_arguments parsed_args{};
  int arg_idx = 1;

  // parse options
  while (arg_idx < argc && argv[arg_idx][0] == '-') {
    if (is_help_flag(argv[arg_idx])) {
      parsed_args.help = true;
      return parsed_args;
    } else if (is_append_flag(argv[arg_idx])) {
      parsed_args.append = true;
    } else if (is_bit_depth_flag(argv[arg_idx])) {
      parsed_args.bit_depth = next_arg_to_int(argc, argv, arg_idx, "bit_depth");
      ++arg_idx;
    } else if (is_word_size_flag(argv[arg_idx])) {
      parsed_args.word_size = next_arg_to_int(argc, argv, arg_idx, "word_size");
      ++arg_idx;
    } else {
      throw Argument_parse_exception(fmt::format(
          "Unrecognized optional argument provided: {}",
          argv[arg_idx]);
    }
    ++arg_idx;
  }

  // check mutually exclusive options
  if (parsed_args.append) {
    const char* prohibited_flag = nullptr;
    if (parsed_args.bit_depth) {
      prohibited_flag = "bit_depth";
    } else if (parsed_args.word_size) {
      prohibited_flag = "word_size";
    }

    if (prohibited_flag) {
      throw Argument_parse_exception(fmt::format(
          "Cannot accept both the append flag and the {} flag, as the {} would "
          "have already been written to the block device.",
          prohibited_flag, prohibited_flag));
    }
  }

  // parse block device
  if (arg_idx < argc) {
    parsed_args.block_device = argv[arg_idx];
    ++arg_idx;
  } else {
    throw Argument_parse_exception(
        "A positional argument naming the block device to write to must be "
        "provided.");
  }

  // parse input wav files
  if (arg_idx < argc) {
    using size_type = decltype(parsed_args.audio_files)::size_type;
    parsed_args.audio_files.reserve(static_cast<size_type>(argc - arg_idx - 1));
    while (arg_idx < argc) {
      parsed_args.audio_files.emplace_back(argv[arg_idx]);
      ++arg_idx;
    }
  } else {
    throw Argument_parse_exception(
        "At least one input wav file must be provided.");
  }

  return parsed_args;
}

}  // namespace cyrus
