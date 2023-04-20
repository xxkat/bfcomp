#pragma once

#include <cstdint>
#include <memory>

#include <emit.h>

namespace bfcomp {

enum class Prefix : uint8_t {
  kLock                = 0xf0,
  kRepne               = 0xf2,
  kRep                 = 0xf3,
  kSegmentOverrideCs   = 0x2e,
  kSegmentOverrideSs   = 0x36,
  kSegmentOverrideDs   = 0x3e,
  kSegmentOverrideEs   = 0x26,
  kSegmentOverrideFs   = 0x64,
  kSegmentOverrideGs   = 0x65,
  kOperandSizeOverride = 0x66,
  kAddressSizeOverride = 0x67,
};

enum class Register : uint8_t {
  kRax,
  kRcx,
  kRdx,
  kRbx,
  kRsp,
  kRbp,
  kRsi,
  kRdi,
  kR8,
  kR9,
  kR10,
  kR11,
  kR12,
  kR13,
  kR14,
  kR15,
};

enum class Condition : uint8_t {
  kOverflow,
  kNotOverflow,
  kBelow,
  kNotBelow,
  kEqual,
  kNotEqual,
  kBelowOrEqual,
  kNotBelowOrEqual,
  kSign,
  kNotSign,
  kParityEven,
  kParityOdd,
  kLess,
  kNotLess,
  kLessOrEqual,
  kNotLessOrEqual,
};

namespace impl {

constexpr uint8_t reg_to_int(Register reg) {
  return std::to_underlying(reg) & 0x7;
}

constexpr bool is_extended(Register reg) {
  return reg >= Register::kR8;
}

constexpr uint8_t make_rex(bool w, bool r, bool x, bool b) {
  return 0x40 | w << 3 | r << 2 | x << 1 | b;
}

constexpr uint8_t make_modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
  return mod << 6 | reg << 3 | rm;
}

constexpr uint8_t make_sib(int scale, uint8_t index, uint8_t base) {
  return (scale & 0x3) << 6 | (index & 0x7) << 3 | (base & 0x7);
}

}  // namespace impl

struct BackPatch {
  size_t m_rip{};
  size_t m_size{};
  size_t m_ins_len{};
  ptrdiff_t m_offset{};
};

struct Label {
  size_t relative_to(size_t rip, size_t ins_len) const {
    return m_rip - rip - ins_len;
  }

  bool is_bound() const {
    return m_bound;
  }

  void bind(size_t rip) {
    m_bound = true;
    m_rip   = rip;
  }

  void add_backpatch(Emitter& emit, size_t op_size, size_t ins_len, ptrdiff_t offset = 0) {
    m_patches.emplace_back(BackPatch{
        .m_rip     = emit.cursor(),
        .m_size    = op_size,
        .m_ins_len = ins_len,
        .m_offset  = offset,
    });
  }

  void resolve_backpatches(Emitter& emit) {
    if (m_patches.empty() || !m_bound) {
      return;
    }
    for (auto it{m_patches.begin()}; it != m_patches.end(); ++it) {
      const auto disp{relative_to(it->m_rip, it->m_ins_len)};
      if (it->m_size == 1) {
        emit.emit_to<int8_t>(it->m_rip + it->m_offset, disp);
      } else if (it->m_size == 4) {
        emit.emit_to<int32_t>(it->m_rip + it->m_offset, disp);
      } else {
        std::unreachable();
      }
    }
    m_patches.clear();
  }

 private:
  std::vector<BackPatch> m_patches{};
  bool m_bound{};
  size_t m_rip{};
};

struct Assembler : ForbidCopy {
  explicit Assembler(Emitter& output) : m_emitter{output} {}

  void emit_jcc(Condition cc, Label* label) {
    const auto emit_real{[&](int32_t disp) {
      m_emitter.emit<uint8_t>(0x0f);
      m_emitter.emit<uint8_t>(0x80 + std::to_underlying(cc));
      m_emitter.emit<int32_t>(disp);
    }};
    if (!label->is_bound()) {
      label->add_backpatch(m_emitter, 4, 6, 2);
      emit_real(0x00);
    } else {
      emit_real(label->relative_to(m_emitter.cursor(), 6));
    }
  }

  void emit_jmp(Label* label) {
    const auto emit_real{[&](int32_t disp) {
      m_emitter.emit<uint8_t>(0xe9);
      m_emitter.emit<int32_t>(disp);
    }};
    if (!label->is_bound()) {
      label->add_backpatch(m_emitter, 4, 5, 1);
      emit_real(0x00);
    } else {
      emit_real(label->relative_to(m_emitter.cursor(), 5));
    }
  }

  void emit_ret(int16_t imm = 0) {
    if (!imm) {
      m_emitter.emit<uint8_t>(0xc3);
    } else {
      m_emitter.emit<uint8_t>(0xc2);
      m_emitter.emit<uint16_t>(imm);
    }
  }

  void emit_push(Register reg) {
    m_emitter.emit<uint8_t>(0x50 + impl::reg_to_int(reg));
  }

  void emit_pop(Register reg) {
    m_emitter.emit<uint8_t>(0x58 + impl::reg_to_int(reg));
  }

  void emit_sub_ri32(Register reg, int32_t imm) {
    m_emitter.emit<uint8_t>(impl::make_rex(true, false, false, impl::is_extended(reg)));
    m_emitter.emit<uint8_t>(0x81);
    m_emitter.emit<uint8_t>(impl::make_modrm(3, 5, impl::reg_to_int(reg)));
    m_emitter.emit<int32_t>(imm);
  }

  void emit_mov_ri32(Register reg, int32_t imm) {
    m_emitter.emit<uint8_t>(impl::make_rex(true, false, false, impl::is_extended(reg)));
    m_emitter.emit<uint8_t>(0xc7);
    m_emitter.emit<uint8_t>(impl::make_modrm(3, 0, impl::reg_to_int(reg)));
    m_emitter.emit<int32_t>(imm);
  }

  void emit_mov_rr(Register dst, Register src) {
    m_emitter.emit<uint8_t>(impl::make_rex(true, impl::is_extended(src), false, impl::is_extended(dst)));
    m_emitter.emit<uint8_t>(0x89);
    m_emitter.emit<uint8_t>(impl::make_modrm(3, impl::reg_to_int(src), impl::reg_to_int(dst)));
  }

  void emit_xor_rr(Register dst, Register src) {
    m_emitter.emit<uint8_t>(impl::make_rex(true, impl::is_extended(src), false, impl::is_extended(dst)));
    m_emitter.emit<uint8_t>(0x31);
    m_emitter.emit<uint8_t>(impl::make_modrm(3, impl::reg_to_int(src), impl::reg_to_int(dst)));
  }

  void emit_dec_r(Register reg) {
    m_emitter.emit<uint8_t>(impl::make_rex(true, false, false, impl::is_extended(reg)));
    m_emitter.emit<uint8_t>(0xff);
    m_emitter.emit<uint8_t>(impl::make_modrm(3, 1, impl::reg_to_int(reg)));
  }

  void emit_inc_r(Register reg) {
    m_emitter.emit<uint8_t>(impl::make_rex(true, false, false, impl::is_extended(reg)));
    m_emitter.emit<uint8_t>(0xff);
    m_emitter.emit<uint8_t>(impl::make_modrm(3, 0, impl::reg_to_int(reg)));
  }

  void emit_dec_rm8(Register reg) {
    if (impl::is_extended(reg)) {
      m_emitter.emit<uint8_t>(impl::make_rex(true, false, false, true));
    }
    m_emitter.emit<uint8_t>(0xfe);
    m_emitter.emit<uint8_t>(impl::make_modrm(0, 1, impl::reg_to_int(reg)));
    if (reg == Register::kRsp || reg == Register::kR12) {
      m_emitter.emit<uint8_t>(impl::make_sib(0, 4, impl::reg_to_int(reg)));
    }
  }

  void emit_inc_rm8(Register reg) {
    if (impl::is_extended(reg)) {
      m_emitter.emit<uint8_t>(impl::make_rex(true, false, false, true));
    }
    m_emitter.emit<uint8_t>(0xfe);
    m_emitter.emit<uint8_t>(impl::make_modrm(0, 0, impl::reg_to_int(reg)));
    if (reg == Register::kRsp || reg == Register::kR12) {
      m_emitter.emit<uint8_t>(impl::make_sib(0, 4, impl::reg_to_int(reg)));
    }
  }

  void emit_cmp_mi8(Register reg, int32_t value) {
    if (impl::is_extended(reg)) {
      m_emitter.emit<uint8_t>(impl::make_rex(true, false, false, true));
    }
    m_emitter.emit<uint8_t>(0x80);
    m_emitter.emit<uint8_t>(impl::make_modrm(0, 7, impl::reg_to_int(reg)));
    if (reg == Register::kRsp || reg == Register::kR12) {
      m_emitter.emit<uint8_t>(impl::make_sib(0, 4, impl::reg_to_int(reg)));
    }
    m_emitter.emit<int8_t>(value);
  }

  void emit_stosb() {
    m_emitter.emit<uint8_t>(0xaa);
  }

  void emit_prefix(Prefix type) {
    m_emitter.emit<uint8_t>(std::to_underlying(type));
  }

  void emit_cld() {
    m_emitter.emit<uint8_t>(0xfc);
  }

  void emit_int3() {
    m_emitter.emit<uint8_t>(0xcc);
  }

  Label* label() {
    return m_labels.emplace_back(std::make_unique<Label>()).get();
  }

  void bind(Label* label) {
    label->bind(m_emitter.cursor());
  }

  std::span<const uint8_t> finalize() {
    for (const auto& label : m_labels) {
      label->resolve_backpatches(m_emitter);
    }
    m_labels.clear();
    return m_emitter.as_view();
  }

 private:
  std::vector<std::unique_ptr<Label>> m_labels;
  Emitter& m_emitter;
};

}  // namespace bfcomp
