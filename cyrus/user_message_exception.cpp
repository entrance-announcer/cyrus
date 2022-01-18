#include <cyrus/user_message_exception.hpp>

namespace cyrus {

User_message_exception::User_message_exception(const std::string& what_arg)
    : std::runtime_error(what_arg) {}

User_message_exception::User_message_exception(const char* what_arg)
    : std::runtime_error(what_arg) {}

}  // namespace cyrus
