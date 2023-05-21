#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace bfcomp {

template <typename T>
concept Serializable = std::is_trivial_v<std::remove_cvref_t<T>>;

template <Serializable T>
constexpr auto to_bytes(T&& value) {
  return std::bit_cast<std::array<uint8_t, sizeof(T)>>(value);
}

struct DataBuffer {
  template <Serializable T>
  void emit(T&& value) {
    emit_range(to_bytes(std::forward<T>(value)));
  }

  template <std::ranges::range T>
  void emit_range(T&& value) {
    m_storage.append_range(std::forward<T>(value));
  }

  template <Serializable T>
  void put(size_t offset, T&& value) {
    const auto dest{std::next(m_storage.begin(), offset)};
    std::ranges::copy(to_bytes(std::forward<T>(value)), dest);
  }

  std::span<const uint8_t> as_view() const {
    return m_storage;
  }

  size_t cursor() const {
    return m_storage.size();
  }

 private:
  std::vector<uint8_t> m_storage{};
};

}  // namespace bfcomp
