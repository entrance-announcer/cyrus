#pragma once

#include <stdexcept>
#include <string>

namespace cyrus {

class User_message_exception : public std::runtime_error {
 public:
  explicit User_message_exception(const std::string&);
  explicit User_message_exception(const char*);
};

}  // namespace cyrus
