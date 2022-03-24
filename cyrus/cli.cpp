#include <fmt/format.h>

#include <charconv>
#include <cyrus/cli.hpp>
#include <cyrus/try.hpp>
#include <iostream>
#include <string>
#include <tl/expected.hpp>
#include <utility>

namespace cyrus {

namespace {

struct Flags_t {
  const char* const short_flag{nullptr};
  const char* const long_flag{nullptr};
};
constexpr Flags_t help_flags{"-h", "--help"};
constexpr Flags_t bit_depth_flags{"-b", "--bit_depth"};
constexpr Flags_t word_size_flags{"-w", "--word_size"};
constexpr Flags_t sample_rate_flags{"-s", "--sample_rate"};
constexpr Flags_t maximize_flags{"-m", "--maximize"};

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
    "{help} {help_long}\tShow this help message and exit\n"
    "{bit} {bit_long} <int>\tNumber of bits used to store written samples [Default {bit_default}]\n"
    "{word} {word_long} <int>\tNumber of bytes per written word [Default {word_default}]\n"
    "{rate} {rate_long} <int>\tSamples/second of written audio [Default {rate_default}]\n"
    "{max} {max_long}\t\tScale the input waveform to occupy the entire bit_depth\n";
// clang-format on

[[nodiscard]] bool is_flag(const Flags_t flags, const Program_argument arg) noexcept {
  return arg == flags.short_flag || arg == flags.long_flag;
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
    if (is_flag(help_flags, *prog_arg_it)) {
      parsed_opts.help = true;
      break;
    } else if (is_flag(bit_depth_flags, *prog_arg_it)) {
      parsed_opts.bit_depth = TRY(next_arg_to_int({prog_arg_it, last}, "bit_depth"));
      ++prog_arg_it;
    } else if (is_flag(word_size_flags, *prog_arg_it)) {
      parsed_opts.word_size = TRY(next_arg_to_int({prog_arg_it, last}, "word_size"));
      ++prog_arg_it;
    } else if (is_flag(sample_rate_flags, *prog_arg_it)) {
      parsed_opts.sample_rate = TRY(next_arg_to_int({prog_arg_it, last}, "sample_rate"));
      ++prog_arg_it;
    } else if (is_flag(maximize_flags, *prog_arg_it)) {
      parsed_opts.maximize = true;
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
  using namespace fmt::literals;
  return fmt::format(
      help_message_fmt, "help"_a = help_flags.short_flag,
      "help_long"_a = help_flags.long_flag, "bit"_a = bit_depth_flags.short_flag,
      "bit_long"_a = bit_depth_flags.long_flag, "bit_default"_a = default_bit_depth,
      "word"_a = word_size_flags.short_flag, "word_long"_a = word_size_flags.long_flag,
      "word_default"_a = default_word_size, "rate"_a = sample_rate_flags.short_flag,
      "rate_long"_a = sample_rate_flags.long_flag, "rate_default"_a = default_sample_rate,
      "max"_a = maximize_flags.short_flag, "max_long"_a = maximize_flags.long_flag);
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
