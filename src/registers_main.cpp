#include <iostream>
#include "registers.hpp"

int main() {
    arm64::Registers regs;

    // (Optionally set a few values here to demo—commented out to match sample all-zeros)
    // regs.writeX(0, 0x1234);
    // regs.writeSP(0x1000);
    // regs.writePC(0x2000);
    // regs.state().Z = true;

    regs.print(std::cout);
    return 0;
}
