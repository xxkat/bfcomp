#include <cstdint>
#include <cstdlib>

#include <jit.h>
#include <util.h>

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    bfcomp::print("Usage: {} <Inline Source>", *argv);
    return EXIT_FAILURE;
  } else {
    bfcomp::execute(argv[1]);
    return EXIT_SUCCESS;
  }
}
