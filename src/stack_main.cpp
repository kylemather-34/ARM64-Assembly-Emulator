#include <iostream>
#include "registers.hpp"  // Task 3 uses Task 2's registers.
#include "stack.hpp"

int main(int argc, char** argv) {
    // Create stack and registers
    arm64::Stack stack(/*baseAddress=*/0x0);
    arm64::Registers registers;

    // Set SP to the base of the stack before executing
    registers.writeSP(stack.base());

    // Print just the stack, formatted like task example output
    stack.printDump(std::cout);

    return 0;
}
