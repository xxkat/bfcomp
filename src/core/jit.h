#pragma once

#include <cstdint>

#include <core/x86.h>
#include <util/print.h>

namespace bfcomp {

constexpr static size_t kStackReserve{512};

struct Branch {
  explicit Branch(X86Assembler& cc) : m_begin{cc.add_label()}, m_end{cc.add_label()} {}

  void start(X86Assembler& cc) {
    cc.bind_label(m_begin);
    cc.emit_cmp_mi8(Register::kRdi, 0);
    cc.emit_jcc(Condition::kEqual, m_end);
  }

  void close(X86Assembler& cc) {
    cc.emit_jmp(m_begin);
    cc.bind_label(m_end);
  }

 private:
  Label *m_begin{}, *m_end{};
};

inline void gen_prologue(X86Assembler& cc) {
  cc.emit_push(Register::kRbp);
  cc.emit_push(Register::kRdi);

  cc.emit_mov_rr(Register::kRbp, Register::kRsp);
  cc.emit_sub_ri32(Register::kRsp, kStackReserve);

  cc.emit_mov_ri32(Register::kRcx, kStackReserve);
  cc.emit_mov_rr(Register::kRdi, Register::kRsp);
  cc.emit_xor_rr(Register::kRax, Register::kRax);

  cc.emit_cld();
  cc.emit_prefix(Prefix::kRepne);
  cc.emit_stosb();

  cc.emit_dec_r(Register::kRdi);
}

inline void gen_epilogue(X86Assembler& cc) {
  cc.emit_mov_rr(Register::kRsp, Register::kRbp);
  cc.emit_pop(Register::kRdi);
  cc.emit_pop(Register::kRbp);
  cc.emit_ret();
}

template <typename T>
void gen_native_call(X86Assembler& cc, Label* label, T func) {
  cc.bind_label(label);
  cc.emit_movabs(Register::kRax, reinterpret_cast<uintptr_t>(func));
  cc.emit_call_r(Register::kRax);
  cc.emit_ret();
}

inline void jit_compile(X86Assembler& cc, std::string_view code) {
  Label* putchar_label{cc.add_label()};
  std::vector<Branch> branch_stack{};

  gen_prologue(cc);
  {
    for (const auto ch : code) {
      switch (ch) {
        case '>':
          cc.emit_dec_r(Register::kRdi);
          break;
        case '<':
          cc.emit_inc_r(Register::kRdi);
          break;
        case '[':
          branch_stack.emplace_back(Branch{cc}).start(cc);
          break;
        case ']':
          branch_stack.back().close(cc);
          branch_stack.pop_back();
          break;
        case '+':
          cc.emit_inc_rm8(Register::kRdi);
          break;
        case '-':
          cc.emit_dec_rm8(Register::kRdi);
          break;
        case '.':
          cc.emit_movzx_rm8(Register::kRcx, Register::kRdi);
          cc.emit_call(putchar_label);
          cc.emit_xor_rr(Register::kRax, Register::kRax);
          break;
        case ',':
          cc.emit_int3();
          break;
      }
    }
  }
  gen_epilogue(cc);
  gen_native_call(cc, putchar_label, std::putchar);
}

}  // namespace bfcomp
