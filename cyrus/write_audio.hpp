#pragma once

#include <cyrus/cli.hpp>
#include <string>
#include <tl/expected.hpp>

namespace cyrus {

tl::expected<void, std::string> write_audio_to_device(const Parsed_arguments&);

}  // namespace cyrus
