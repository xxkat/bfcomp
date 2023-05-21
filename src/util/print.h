#pragma once

#include <format>
#include <iostream>
#include <string_view>

namespace bfcomp {

template <typename... Args>
void print(std::ostream& stream, std::format_string<Args...> fmt, Args&&... args) {
  std::format_to(std::ostream_iterator<char>{stream}, fmt, std::forward<Args>(args)...);
  stream << '\n';
}

template <typename... Args>
void print(std::format_string<Args...> fmt, Args&&... args) {
  return print(std::cout, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void panic(std::format_string<Args...> fmt, Args&&... args) {
  print(std::cerr, fmt, std::forward<Args>(args)...);
  std::exit(EXIT_FAILURE);
}

}  // namespace bfcomp
