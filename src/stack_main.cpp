#include <iostream>
#include "registers.hpp"  // from Task 2
#include "stack.hpp"

int main(int argc, char** argv) {
    // Create stack and registers
    arm64::Stack stack(/*base_address=*/0x0);
    arm64::Registers regs;

    // Set SP to the stack base before executing anything (per task description)
    regs.writeSP(stack.base());

    // Optional: if user passes --random, show the “random bytes” example
    if (argc > 1 && std::string(argv[1]) == "--random") {
        stack.fill_random();
    }

    // Print just the stack (format exactly like the examples)
    stack.print_dump(std::cout);

    return 0;
}
