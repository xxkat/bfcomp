#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <core/assembler.h>

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

struct BackPatch {
  Label* m_target{};
  size_t m_rip{};
  size_t m_len{};
  size_t m_size{};
  ptrdiff_t m_offset{};
};

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

inline size_t relative_to_label(const Label* l, size_t rip, size_t len) {
  return l->rip() - rip - len;
}

struct X86Assembler final : BaseAssembler {
  void add_backpatch(Label* l, size_t instruction_length, size_t data_size, ptrdiff_t offset) {
    if (!l->bound()) {
      m_patches.emplace_back(l, cursor(), instruction_length, data_size, offset);
    }
  }

  void backpatch() {
    for (auto it{m_patches.begin()}; it != m_patches.end(); ++it) {
      const auto disp{it->m_target->rip() - it->m_rip - it->m_len};
      const auto loc{it->m_rip + it->m_offset};

      if (it->m_size == 1) {
        put<int8_t>(loc, disp);
      } else if (it->m_size == 2) {
        put<int16_t>(loc, disp);
      } else if (it->m_size == 4) {
        put<int32_t>(loc, disp);
      } else {
        std::unreachable();
      }
    }

    m_patches.clear();
  }

  std::span<const uint8_t> finalize() override {
    backpatch();
    return as_view();
  }

  void emit_int3() {
    emit<uint8_t>(0xcc);
  }

  void emit_push(Register reg) {
    emit<uint8_t>(0x50 + reg_to_int(reg));
  }

  void emit_pop(Register reg) {
    emit<uint8_t>(0x58 + reg_to_int(reg));
  }

  void emit_sub_ri32(Register reg, int32_t imm) {
    emit<uint8_t>(make_rex(true, false, false, is_extended(reg)));
    emit<uint8_t>(0x81);
    emit<uint8_t>(make_modrm(3, 5, reg_to_int(reg)));
    emit(imm);
  }

  void emit_add_ri32(Register reg, int32_t imm) {
    emit<uint8_t>(make_rex(true, false, false, is_extended(reg)));
    emit<uint8_t>(0x81);
    emit<uint8_t>(make_modrm(3, 0, reg_to_int(reg)));
    emit(imm);
  }

  void emit_mov_ri32(Register reg, int32_t imm) {
    emit<uint8_t>(make_rex(true, false, false, is_extended(reg)));
    emit<uint8_t>(0xc7);
    emit<uint8_t>(make_modrm(3, 0, reg_to_int(reg)));
    emit(imm);
  }

  void emit_movabs(Register reg, int64_t imm) {
    emit<uint8_t>(make_rex(true, false, false, is_extended(reg)));
    emit<uint8_t>(0xb8 + reg_to_int(reg));
    emit(imm);
  }

  void emit_mov_rr(Register dst, Register src) {
    emit<uint8_t>(make_rex(true, is_extended(src), false, is_extended(dst)));
    emit<uint8_t>(0x89);
    emit<uint8_t>(make_modrm(3, reg_to_int(src), reg_to_int(dst)));
  }

  void emit_movzx_rm8(Register dst, Register src) {
    emit<uint8_t>(make_rex(true, is_extended(dst), false, is_extended(src)));
    emit<uint16_t>(0xb60f);
    emit<uint8_t>(make_modrm(0, reg_to_int(dst), reg_to_int(src)));
  }

  void emit_xor_rr(Register dst, Register src) {
    emit<uint8_t>(make_rex(true, is_extended(src), false, is_extended(dst)));
    emit<uint8_t>(0x31);
    emit<uint8_t>(make_modrm(3, reg_to_int(src), reg_to_int(dst)));
  }

  void emit_dec_r(Register reg) {
    emit<uint8_t>(make_rex(true, false, false, is_extended(reg)));
    emit<uint8_t>(0xff);
    emit<uint8_t>(make_modrm(3, 1, reg_to_int(reg)));
  }

  void emit_inc_r(Register reg) {
    emit<uint8_t>(make_rex(true, false, false, is_extended(reg)));
    emit<uint8_t>(0xff);
    emit<uint8_t>(make_modrm(3, 0, reg_to_int(reg)));
  }

  void emit_dec_rm8(Register reg) {
    emit<uint8_t>(0xfe);
    emit<uint8_t>(make_modrm(0, 1, reg_to_int(reg)));
  }

  void emit_inc_rm8(Register reg) {
    emit<uint8_t>(0xfe);
    emit<uint8_t>(make_modrm(0, 0, reg_to_int(reg)));
  }

  void emit_cmp_mi8(Register reg, int32_t value) {
    emit<uint8_t>(0x80);
    emit<uint8_t>(make_modrm(0, 7, reg_to_int(reg)));
    emit<int8_t>(value);
  }

  void emit_jcc(Condition cc, Label* l) {
    const auto rip{cursor()};
    add_backpatch(l, 6, 4, 2);
    emit<uint8_t>(0x0f);
    emit<uint8_t>(0x80 + std::to_underlying(cc));
    emit<int32_t>(l->relative_to(rip, 6));
  }

  void emit_jmp(Label* l) {
    const auto rip{cursor()};
    add_backpatch(l, 5, 4, 1);
    emit<uint8_t>(0xe9);
    emit<int32_t>(l->relative_to(rip, 5));
  }

  void emit_call(Label* l) {
    const auto rip{cursor()};
    add_backpatch(l, 5, 4, 1);
    emit<uint8_t>(0xe8);
    emit<int32_t>(l->relative_to(rip, 5));
  }

  void emit_call_r(Register reg) {
    emit<uint8_t>(0xff);
    emit<uint8_t>(0xd0 + reg_to_int(reg));
  }

  void emit_ret(int16_t imm = 0) {
    if (!imm) {
      emit<uint8_t>(0xc3);
    } else {
      emit<uint8_t>(0xc2);
      emit(imm);
    }
  }

  void emit_prefix(Prefix type) {
    emit<uint8_t>(std::to_underlying(type));
  }

  void emit_stosb() {
    emit<uint8_t>(0xaa);
  }

  void emit_cld() {
    emit<uint8_t>(0xfc);
  }

 private:
  std::vector<BackPatch> m_patches{};
};

}  // namespace bfcomp
