#pragma once

#include <span>
#include <string_view>

namespace cyrus {

using Program_arguments = std::span<const std::string_view>;
using Program_argument = Program_arguments::value_type;

int cmain(Program_arguments args);

}  // namespace cyrus
