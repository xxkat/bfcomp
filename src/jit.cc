#include <Windows.h>

#include <asm.h>
#include <jit.h>
#include <util.h>

namespace {

constexpr static size_t kStackReserve{512};

struct Branch {
  explicit Branch(bfcomp::Assembler& cc) : m_begin{cc.label()}, m_end{cc.label()} {}

  void start(bfcomp::Assembler& cc) {
    cc.bind(m_begin);
    cc.emit_cmp_mi8(bfcomp::Register::kRsp, 0);
    cc.emit_jcc(bfcomp::Condition::kEqual, m_end);
  }

  void close(bfcomp::Assembler& cc) {
    cc.emit_jmp(m_begin);
    cc.bind(m_end);
  }

 private:
  bfcomp::Label *m_begin{}, *m_end{};
};

void gen_prologue(bfcomp::Assembler& cc) {
  cc.emit_push(bfcomp::Register::kRbp);
  cc.emit_push(bfcomp::Register::kRdi);
  cc.emit_mov_rr(bfcomp::Register::kRbp, bfcomp::Register::kRsp);
  cc.emit_sub_ri32(bfcomp::Register::kRsp, kStackReserve);
  cc.emit_mov_ri32(bfcomp::Register::kRcx, kStackReserve);
  cc.emit_mov_rr(bfcomp::Register::kRdi, bfcomp::Register::kRsp);
  cc.emit_xor_rr(bfcomp::Register::kRax, bfcomp::Register::kRax);
  cc.emit_cld();
  cc.emit_prefix(bfcomp::Prefix::kRepne);
  cc.emit_stosb();
}

void gen_epilogue(bfcomp::Assembler& cc) {
  cc.emit_mov_rr(bfcomp::Register::kRsp, bfcomp::Register::kRbp);
  cc.emit_pop(bfcomp::Register::kRdi);
  cc.emit_pop(bfcomp::Register::kRbp);
  cc.emit_ret();
}

void compile(bfcomp::Assembler& cc, std::string_view code) {
  std::vector<Branch> branch_stack{};
  gen_prologue(cc);
  {
    for (const auto ch : code) {
      switch (ch) {
        case '>':
          cc.emit_inc_r(bfcomp::Register::kRsp);
          break;
        case '<':
          cc.emit_dec_r(bfcomp::Register::kRsp);
          break;
        case '[':
          branch_stack.emplace_back(Branch{cc}).start(cc);
          break;
        case ']':
          branch_stack.back().close(cc);
          branch_stack.pop_back();
          break;
        case '+':
          cc.emit_inc_rm8(bfcomp::Register::kRsp);
          break;
        case '-':
          cc.emit_dec_rm8(bfcomp::Register::kRsp);
          break;
        case '.':
        case ',':
          cc.emit_int3();
          break;
      }
    }
  }
  gen_epilogue(cc);
}

}  // namespace

void bfcomp::execute(std::string_view source) {
  using BfEntry = void (*)();

  Emitter emit;
  Assembler cc{emit};
  compile(cc, source);

  const auto code{cc.finalize()};
  const auto alloc{VirtualAlloc(nullptr, code.size(), MEM_COMMIT, PAGE_EXECUTE_READWRITE)};

  if (!alloc) {
    print("Failed to allocate memory!");
    std::exit(EXIT_FAILURE);
  } else {
    std::memcpy(alloc, code.data(), code.size());
    reinterpret_cast<BfEntry>(alloc)();
    VirtualFree(alloc, 0, MEM_RELEASE);
  }
}
