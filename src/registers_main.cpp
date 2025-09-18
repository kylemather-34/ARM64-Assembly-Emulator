#include <iostream>
#include "registers.hpp"

int main() {
    arm64::Registers regs;

    regs.print(std::cout); // Print initial state of registers
    return 0;
}
