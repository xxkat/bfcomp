#include <Windows.h>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <core/jit.h>
#include <util/print.h>

namespace {

void jit_execute(std::string_view source) {
  using BfEntry = void (*)();

  bfcomp::X86Assembler cc{};
  bfcomp::jit_compile(cc, source);

  const auto code{cc.finalize()};
  const auto alloc{VirtualAlloc(nullptr, code.size(), MEM_COMMIT, PAGE_EXECUTE_READWRITE)};

  if (!alloc) {
    bfcomp::panic("Failed to allocate memory!");
  } else {
    std::memcpy(alloc, code.data(), code.size());
    reinterpret_cast<BfEntry>(alloc)();
    VirtualFree(alloc, 0, MEM_RELEASE);
  }
}

void jit_from_file(const std::filesystem::path& file_path) {
  std::ifstream fs{file_path};

  if (!fs.is_open()) {
    bfcomp::panic("Unable to open input file {}!", file_path.string());
  }

  return jit_execute(std::string{
      std::istreambuf_iterator<char>{fs},
      std::istreambuf_iterator<char>{},
  });
}

}  // namespace

int32_t main(int argc, const char* argv[]) {
  if (argc < 2) {
    bfcomp::print("Usage: {} <file-path>", *argv);
    return EXIT_FAILURE;
  } else {
    jit_from_file(argv[1]);
    return EXIT_SUCCESS;
  }
}
