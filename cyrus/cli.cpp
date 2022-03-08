#include <fmt/format.h>

#include <charconv>
#include <cyrus/cli.hpp>
#include <cyrus/try.hpp>
#include <iostream>
#include <string>
#include <tl/expected.hpp>

namespace cyrus {

namespace {

// clang-format off
constexpr const char* const help_message_fmt =
    "Usage: cyrus [options] <block_device> <audio_files...>\n"
    " Write the provided wav and aiff files to the block device\n"
    "\n"
    "Ex. 1: cyrus /dev/nvme0n1 ordinary_girl.aiff nobodys_perfect.wav who_said.wav\n"
    "Ex. 2: cyrus -b=24 -w=2 /dev/nvme0n1 he_coule_be_the_one.aif\n"
    "\n"
    "Positional Arguments:\n"
    "block_device\tDestination block device\n"
    "audio_files\tInput wav and aiff formatted audio files\n"
    "\n"
    "Optional Arguments:\n"
    "-h --help\tShow this help message and exit\n"
    "\t\tProhibits the use of -b, -w, and -s flags, for these values are already stored on disk.\n"
    "-b --bit_depth <int>\tNumber of bits to store written samples [Default {}]\n"
    "-w --word_size <int>\tNumber of bytes per written word [Default {}]\n"
    "-s --_sample_rate <int>\tSamples/second to write audio [Default {}]\n";
// clang-format on

[[nodiscard]] bool is_help_flag(const Program_argument arg) noexcept {
  return arg == "-h" || arg == "--help";
}

[[nodiscard]] bool is_bit_depth_flag(const Program_argument arg) noexcept {
  return arg.starts_with("-b") || arg.starts_with("--bit_depth");
}

[[nodiscard]] bool is_word_size_flag(const Program_argument arg) noexcept {
  return arg.starts_with("-w") || arg.starts_with("--word_size");
}

[[nodiscard]] bool is_sample_rate_flag(const Program_argument arg) noexcept {
  return arg.starts_with("-s") || arg.starts_with("--_sample_rate");
}

[[nodiscard]] tl::expected<int, std::string> next_arg_to_int(
    const Program_arguments prog_args, const std::string_view option_name) {
  if (prog_args.size() < 2) {
    return tl::make_unexpected(
        fmt::format("Expected an integer value following the provided {} flag, {}.",
                    option_name, prog_args.front()));
  }

  // parse the argument to an int
  int parsed_int;
  const auto int_arg = *(prog_args.begin() + 1);
  if (const auto result =
          std::from_chars(int_arg.data(), int_arg.data() + int_arg.size(), parsed_int);
      result.ec != std::errc{}) {
    return tl::make_unexpected(
        fmt::format("Failed parsing argument {} to an integer for option {}: {}\n",
                    result.ptr, option_name, std::strerror(static_cast<int>(result.ec))));
  }
  return parsed_int;
}

struct Parse_context {
  Program_arguments prog_args{};
  Parsed_arguments parsed_args{};
};

[[nodiscard]] tl::expected<Parse_context, std::string> strip_exec_name(
    const Parse_context& ctx) {
  return Parse_context{.prog_args = {ctx.prog_args.begin() + 1, ctx.prog_args.end()},
                       .parsed_args = ctx.parsed_args};
}

[[nodiscard]] tl::expected<Parse_context, std::string> parse_options(Parse_context ctx) {
  auto& parsed_opts = ctx.parsed_args;
  auto prog_arg_it = ctx.prog_args.begin();
  const auto last = ctx.prog_args.end();
  for (; prog_arg_it < ctx.prog_args.end() && prog_arg_it->front() == '-';
       ++prog_arg_it) {
    if (is_help_flag(*prog_arg_it)) {
      parsed_opts.help = true;
      break;
    } else if (is_bit_depth_flag(*prog_arg_it)) {
      parsed_opts.bit_depth = TRY(next_arg_to_int({prog_arg_it, last}, "bit_depth"));
      ++prog_arg_it;
    } else if (is_word_size_flag(*prog_arg_it)) {
      parsed_opts.word_size = TRY(next_arg_to_int({prog_arg_it, last}, "word_size"));
      ++prog_arg_it;
    } else if (is_sample_rate_flag(*prog_arg_it)) {
      parsed_opts.sample_rate = TRY(next_arg_to_int({prog_arg_it, last}, "_sample_rate"));
      ++prog_arg_it;
    } else {
      return tl::make_unexpected(
          fmt::format("Unrecognized optional argument provided: {}", *prog_arg_it));
    }
  }

  return Parse_context{.prog_args = {prog_arg_it, last}, .parsed_args = parsed_opts};
}


[[nodiscard]] tl::expected<Parse_context, std::string> verify_options(
    const Parse_context& ctx) {
  // check that word size is multiple
  // check that word size can store sample size
  static const constexpr auto bits_per_byte = 8;
  if (const auto word_size_bits = ctx.parsed_args.word_size * bits_per_byte;
      ctx.parsed_args.bit_depth > word_size_bits) {
    return tl::make_unexpected(fmt::format(
        "The word size of {} ({} bits) is not large enough to store "
        "samples of {} bits.\n",
        ctx.parsed_args.word_size, word_size_bits, ctx.parsed_args.bit_depth));
  }

  return ctx;
}

[[nodiscard]] tl::expected<Parse_context, std::string> parse_block_device(
    Parse_context ctx) {
  const auto prog_args = ctx.prog_args;
  if (!prog_args.empty()) {
    ctx.parsed_args.block_device = prog_args.front();
  } else {
    return tl::make_unexpected(
        "A positional argument naming the block device to write to must be provided.");
  }

  return Parse_context{.prog_args = {prog_args.begin() + 1, prog_args.end()},
                       .parsed_args = ctx.parsed_args};
}


[[nodiscard]] tl::expected<Parse_context, std::string> parse_audio_files(
    Parse_context ctx) {
  auto& parsed_args = ctx.parsed_args;
  if (const auto prog_args = ctx.prog_args; !prog_args.empty()) {
    parsed_args.audio_files.reserve(prog_args.size());
    for (const auto arg : prog_args) {
      parsed_args.audio_files.emplace_back(arg);
    }
  } else {
    return tl::make_unexpected("At least one input audio file must be provided.");
  }

  return Parse_context{.prog_args = {}, .parsed_args = parsed_args};
}


}  // namespace

std::string help_message() {
  return fmt::format(help_message_fmt, default_bit_depth, default_word_size,
                     default_sample_rate);
}

tl::expected<Parsed_arguments, std::string> parse_arguments(
    const Program_arguments prog_args) {
  return strip_exec_name({prog_args, Parsed_arguments{}})
      .and_then(parse_options)
      .and_then([](auto ctx) {
        if (ctx.parsed_args.help) {
          return tl::expected<Parse_context, std::string>(ctx);
        }
        return verify_options(ctx)
            .and_then(parse_block_device)
            .and_then(parse_audio_files);
      })
      .map([](const auto c) { return c.parsed_args; });
}

bool user_accept_dialog(const std::string_view message, std::string_view accept_option,
                        std::string_view decline_option) {
  static const constexpr char* const trim_chars = " \t\n\r";
  std::string user_input;
  while (true) {
    fmt::print("{}: [{}/{}]: ", message, accept_option, decline_option);
    std::getline(std::cin, user_input);
    user_input.erase(user_input.find_last_not_of(trim_chars) + 1);  // suffixing spaces
    user_input.erase(0, user_input.find_first_not_of(trim_chars));  // prefixing spaces
    if (user_input == accept_option) {
      return true;
    } else if (user_input == decline_option) {
      return false;
    } else {
      fmt::print("Selection '{}' doesn't conform to options of '{}' or '{}'\n",
                 user_input, accept_option, decline_option);
    }
  }
}


}  // namespace cyrus
