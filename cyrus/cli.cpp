#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <concepts>
#include <cyrus/cli.hpp>
#include <cyrus/try.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <tl/expected.hpp>
#include <utility>

namespace cyrus {

namespace {

struct Flags_t {
  const char* const flag{nullptr};
  const char* const long_flag{nullptr};
};
constexpr Flags_t help_flags{"-h", "--help"};
constexpr Flags_t range_flags{"-r", "--out_range"};
constexpr Flags_t word_size_flags{"-w", "--word_size"};
constexpr Flags_t sample_rate_flags{"-s", "--sample_rate"};
constexpr Flags_t enlarge_flags{"-e", "--enlarge"};

// clang-format off
constexpr const char* const help_message_fmt =
    "Usage: cyrus [options] <block_device> <audio_files...>\n"
    " Write the provided audio files to a FAT32 block device in unsigned RAW format\n"
    "\n"
    "Ex. 1: cyrus /dev/nvme0n1 ordinary_girl.aiff nobodys_perfect.wav who_said.wav\n"
    "Ex. 2: cyrus -r 205,3890 -w 2 /dev/nvme0n1 he_coule_be_the_one.aif\n"
    "\n"
    "Positional Arguments:\n"
    "block_device\tDestination block device\n"
    "audio_files\tInput wav and aiff formatted audio files\n"
    "\n"
    "Optional Arguments:\n"
    "{help} {help_long}     \t\tShow this help message and exit\n"
    "{word} {word_long} <int> \tNumber of bytes per written word [Default {word_default}]\n"
    "{range} {range_long} <min,max> Range to generate output samples [Default {range_min_default},{range_max_default}]\n"
    "{rate} {rate_long} <int>\tSamples/second of written audio [Default {rate_default}]\n"
    "{enlarge} {enlarge_long} \t\tEnlarge the input waveform to occupy the entire output range\n";
// clang-format on


[[nodiscard]] bool is_flag(const Flags_t flags, const Program_argument arg) noexcept {
  return arg == flags.flag || arg == flags.long_flag;
}


template <std::integral T>
[[nodiscard]] tl::expected<T, std::from_chars_result> parse_integral(
    std::string_view str) {
  T parsed;
  if (const auto result = std::from_chars(str.data(), str.data() + str.size(), parsed);
      result.ec != std::errc{}) {
    return tl::make_unexpected(result);
  }
  return parsed;
}


[[nodiscard]] tl::expected<int, std::string> next_arg_to_int(
    const Program_arguments prog_args, const std::string_view option_name) {
  if (prog_args.size() < 2) {
    return tl::make_unexpected(
        fmt::format("Expected an integer value following the provided {} flag, {}.",
                    option_name, prog_args.front()));
  }

  // parse the argument to an int
  const auto int_arg = *(prog_args.begin() + 1);
  if (const auto result = parse_integral<int>(int_arg); result) {
    return result.value();
  } else {
    return tl::make_unexpected(
        fmt::format("Failed parsing argument '{}' to an integer for option {}: {}\n",
                    result.error().ptr, option_name,
                    std::strerror(static_cast<int>(result.error().ec))));
  }
}

using Range_type = std::remove_cvref_t<decltype(Parsed_arguments::range_min)>;
static_assert(std::is_same_v<Range_type,
                             std::remove_cvref_t<decltype(Parsed_arguments::range_max)>>,
              "The range argument values must use the same type");

[[nodiscard]] tl::expected<std::pair<Range_type, Range_type>, std::string>
next_arg_to_range(const Program_arguments prog_args, const std::string_view option_name) {
  if (prog_args.size() < 2) {
    return tl::make_unexpected(
        fmt::format("Expected a two comma delimited values following the provided {} "
                    "flag, {}.",
                    option_name, prog_args.front()));
  }

  // split provided argument across comma
  const auto range_arg = *(prog_args.begin() + 1);
  const auto comma_idx = range_arg.find(',');
  if (comma_idx == std::string_view::npos) {
    return tl::make_unexpected(
        fmt::format("The provided {} argument does not contain a comma: {}", option_name,
                    prog_args.front()));
  }
  const auto min_arg = range_arg.substr(0, comma_idx);
  const auto max_arg = range_arg.substr(comma_idx + 1, range_arg.size() - comma_idx - 1);

  // parse the argument to two integrals
  Range_type range_min;
  Range_type range_max;
  for (const auto& [val, arg] :
       {std::pair{&range_min, min_arg}, std::pair{&range_max, max_arg}}) {
    if (auto result = parse_integral<Range_type>(arg); result) {
      *val = result.value();
    } else {
      const auto e = result.error();
      return tl::make_unexpected(
          fmt::format("Failed parsing argument '{}' to an unsigned integral for option "
                      "{}: {}\n",
                      e.ptr, option_name, std::strerror(static_cast<int>(e.ec))));
    }
  }

  return std::pair{range_min, range_max};
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
    } else if (is_flag(word_size_flags, *prog_arg_it)) {
      parsed_opts.word_size = TRY(next_arg_to_int({prog_arg_it, last}, "word_size"));
      ++prog_arg_it;
    } else if (is_flag(sample_rate_flags, *prog_arg_it)) {
      parsed_opts.sample_rate = TRY(next_arg_to_int({prog_arg_it, last}, "sample_rate"));
      ++prog_arg_it;
    } else if (is_flag(enlarge_flags, *prog_arg_it)) {
      parsed_opts.enlarge = true;
    } else if (is_flag(range_flags, *prog_arg_it)) {
      const auto [min, max] = TRY(next_arg_to_range({prog_arg_it, last}, "output_range"));
      parsed_opts.range_min = min;
      parsed_opts.range_max = max;
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
  const auto& parsed = ctx.parsed_args;

  // check that the word size can be written by cyrus
  if (const auto word_sizes = {1, 2, 4, 8};
      std::ranges::find(word_sizes, parsed.word_size) == word_sizes.end()) {
    return tl::make_unexpected(fmt::format(
        "Cannot convert audio samples to a word size of {} bytes", parsed.word_size));
  }

  // check that range is provided in correct order
  if (parsed.range_min > parsed.range_max) {
    return tl::make_unexpected(fmt::format(
        "The output range was specified in reverse order. Was '{},{}', should be "
        "'{},{}'\n",
        parsed.range_min, parsed.range_max, parsed.range_max, parsed.range_min));
  }

  // check the provided enlarge range
  if (const auto word_max = (std::uint64_t(1) << parsed.word_size * 8) - 1u;
      word_max < parsed.range_max) {
    return tl::make_unexpected(
        "Cyrus can only generate unsigned values and the minimum range value can "
        "therefore be no less than 0");
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
      help_message_fmt, "help"_a = help_flags.flag, "help_long"_a = help_flags.long_flag,
      "word"_a = word_size_flags.flag, "word_long"_a = word_size_flags.long_flag,
      "range"_a = range_flags.flag, "range_long"_a = range_flags.long_flag,
      "range_min_default"_a = default_range_min,
      "range_max_default"_a = default_range_max, "word_default"_a = default_word_size,
      "rate"_a = sample_rate_flags.flag, "rate_long"_a = sample_rate_flags.long_flag,
      "rate_default"_a = default_sample_rate, "enlarge"_a = enlarge_flags.flag,
      "enlarge_long"_a = enlarge_flags.long_flag);
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
