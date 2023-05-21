#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include <core/buffer.h>

namespace bfcomp {

struct Label {
  void bind(uintptr_t rip) {
    m_is_bound = true;
    m_rip      = rip;
  }

  size_t relative_to(size_t rip, size_t ins_len) const {
    return m_rip - rip - ins_len;
  }

  uintptr_t rip() const {
    return m_rip;
  }

  bool bound() const {
    return m_is_bound;
  }

 private:
  uintptr_t m_rip{};
  bool m_is_bound{};
};

struct BaseAssembler : DataBuffer {
  Label* add_label() {
    return m_labels.emplace_back(std::make_unique<Label>()).get();
  }

  void bind_label(Label* label) const {
    label->bind(cursor());
  }

  virtual std::span<const uint8_t> finalize() = 0;

 protected:
  std::vector<std::unique_ptr<Label>> m_labels{};
};

}  // namespace bfcomp
