#include <Windows.h>
#include <cstdint>
#include <cstdlib>
#include <fstream>

#include <core/jit.h>
#include <util/print.h>

namespace {

int32_t jit_execute(std::string_view source) {
  using BfEntry = void (*)();

  bfcomp::X86Assembler cc{};
  bfcomp::jit_compile(cc, source);

  const auto code{cc.finalize()};
  const auto alloc{VirtualAlloc(nullptr, code.size(), MEM_COMMIT, PAGE_EXECUTE_READWRITE)};

  if (!alloc) {
    bfcomp::print("Failed to allocate memory!");
    return EXIT_FAILURE;
  } else {
    std::memcpy(alloc, code.data(), code.size());
    reinterpret_cast<BfEntry>(alloc)();
    VirtualFree(alloc, 0, MEM_RELEASE);
  }

  return EXIT_SUCCESS;
}

}  // namespace

int32_t main(int argc, const char* argv[]) {
  if (argc < 2) {
    bfcomp::print("Usage: {} <Inline Source>", *argv);
    return EXIT_FAILURE;
  } else {
    return jit_execute(argv[1]);
  }
}
