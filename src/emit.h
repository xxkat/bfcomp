#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

#include <util.h>

namespace bfcomp {

template <typename T>
concept Serializable = std::is_trivially_copyable_v<T>;

namespace impl {

template <Serializable T>
constexpr auto to_bytes(const T& value) {
  return std::bit_cast<std::array<uint8_t, sizeof(T)>>(value);
}

}  // namespace impl

struct Emitter : ForbidCopy {
  template <Serializable T>
  void emit(T value) {
    m_data.append_range(impl::to_bytes(value));
  }

  template <Serializable T>
  void emit_to(size_t offset, T value) {
    std::ranges::copy(impl::to_bytes(value), std::next(m_data.begin(), offset));
  }

  std::span<const uint8_t> as_view() const {
    return m_data;
  }

  size_t cursor() const {
    return m_data.size();
  }

 private:
  std::vector<uint8_t> m_data;
};

}  // namespace bfcomp
