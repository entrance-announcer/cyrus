#include <cyrus/cyrus_main.hpp>
#include <vector>

int main(const int argc, const char* const argv[]) {
  const std::vector<std::string_view> prog_args(argv, argv + argc);
  return cyrus::cmain({prog_args.begin(), prog_args.size()});
}
