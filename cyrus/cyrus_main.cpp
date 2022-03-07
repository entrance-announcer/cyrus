#include <fmt/core.h>

#include <cyrus/cli.hpp>
#include <cyrus/cyrus_main.hpp>
#include <cyrus/write_audio.hpp>

namespace cyrus {

int cmain(const Program_arguments prog_args) {
  const auto parsed =
      cyrus::parse_arguments(prog_args).map_error([](const auto& err_msg) {
        fmt::print(stderr, "{}\n", err_msg);
        return 1;
      });
  if (!parsed) {
    return parsed.error();
  }
  const auto& parsed_args = parsed.value();
  if (parsed_args.help) {
    fmt::print("\n{}", cyrus::help_message());
    return 0;
  }

  if (const auto w = cyrus::write_audio_to_device(parsed_args); !w) {
    fmt::print(stderr, "{}\n", w.error());
    return 1;
  }

  fmt::print("Done.");
  return 0;
}

}  // namespace cyrus
