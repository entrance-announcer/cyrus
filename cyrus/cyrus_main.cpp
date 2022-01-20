#include <fmt/core.h>

#include <cyrus/cli.hpp>
#include <cyrus/cyrus_main.hpp>
#include <cyrus/write_audio.hpp>

namespace cyrus {

int main(const int argc, const char* const argv[]) {
  cyrus::Parsed_arguments args;

  try {
    args = cyrus::parse_arguments(argc, argv);
  } catch (const cyrus::Argument_parse_error& e) {
    fmt::print(stderr, "{}\n", e.what());
    return 1;
  }

  if (args.help) {
    fmt::print("\n{}", cyrus::help_message());
    return 0;
  }

  try {
    cyrus::write_audio_to_device(args);
  } catch (const cyrus::User_message_exception& e) {
    fmt::print(stderr, "{}\n", e.what());
    return 1;
  }

  return 0;
}

}  // namespace cyrus
