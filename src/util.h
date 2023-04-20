#pragma once

#include <cstdlib>
#include <format>
#include <iostream>

namespace bfcomp {

template <typename... Args>
inline void print(std::format_string<Args...> fmt, Args&&... args) {
  std::format_to(std::ostream_iterator<char>{std::cout}, fmt, std::forward<Args>(args)...);
}

struct ForbidCopy {
  ForbidCopy()                             = default;
  ForbidCopy(const ForbidCopy&)            = delete;
  ForbidCopy& operator=(const ForbidCopy&) = delete;
};

}  // namespace bfcomp
