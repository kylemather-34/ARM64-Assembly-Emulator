/*
* ARM64 Assembly Registers
*
* This header defines the data structures and methods for modeling
* ARM64 CPU registers and processor state flags.
*
* - Implements a full array of X0->X30 general purpose registers.
* - Provides accessors for both 64-bit and 32-bit views.
* - Emulates XZR/WZR behavior.
* - Includes Stack Pointer and Program Counter.
* - Maintains processor state flags for conditional execution.
* - Includes a print() function for human readable registers.
*
* Author: Kyle Mather
*/

#ifndef ARM64_REGISTERS_HPP
#define ARM64_REGISTERS_HPP

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace arm64 {

// Processor state flags
struct ProcessorState {
    bool N{false}; // Boolean for negative
    bool Z{false}; // Boolean for zero
    // C and V flags are not implemented
};



class Registers {
public:
    Registers() {
        x_.fill(0);
        sp_ = 0;
        pc_ = 0;
    }

    // Read/write 64-bit Xn.
    uint64_t readX(unsigned n) const {
        if (n == XZR_INDEX) return 0;
        if (n > 30) throw std::out_of_range("readX: invalid index");
        return x_[n];
    }

    void writeX(unsigned n, uint64_t value) {
        if (n == XZR_INDEX) return; // writes to XZR are ignored
        if (n > 30) throw std::out_of_range("writeX: invalid index");
        x_[n] = value;
    }

    // Read/write 32-bit Wn aliases.
    uint32_t readW(unsigned n) const {
        return static_cast<uint32_t>(readX(n) & 0xFFFF'FFFFull);
    }

    void writeW(unsigned n, uint32_t value) {
        if (n == XZR_INDEX) return; // writes to WZR are ignored
        if (n > 30) throw std::out_of_range("writeW: invalid index");
        x_[n] = static_cast<uint64_t>(value);
    }

    // Stack Pointer and Program Counter
    uint64_t readSP() const { return sp_; }
    void     writeSP(uint64_t v) { sp_ = v; } // Sets stack pointer to specified value

    uint64_t readPC() const { return pc_; }
    void     writePC(uint64_t v) { pc_ = v; } // Sets program counter to specified value

    // Processor state flags
    const ProcessorState& state() const { return psr_; }
    ProcessorState&       state()       { return psr_; }

    // Printing to match the sample format
    void print(std::ostream& os) const {
        static constexpr const char* SEP =
            "-------------------------------------------------------------------------------------------------------------------------------";

        os << SEP << "\n\n";
        os << "Registers:\n\n";
        os << SEP << "\n\n";

        // Print rows in same format as example output
        for (unsigned base = 0; base < 10; ++base) {
            os << "X" << base << ": "   << hex64(readX(base))   << " "
               << "X" << (base+10) << ": " << hex64(readX(base+10)) << " "
               << "X" << (base+20) << ": " << hex64(readX(base+20)) << "\n\n";
        }

        // Final line: SP, PC, X30/LR
        os << "SP: " << hex64(readSP()) << " "
           << "PC: " << hex64(readPC()) << " "
           << "X30: " << hex64(readX(30)) << "\n\n";

        os << "Processor State N bit: " << (psr_.N ? 1 : 0) << "\n\n";
        os << "Processor State Z bit: " << (psr_.Z ? 1 : 0) << "\n";
    }

    static constexpr unsigned XZR_INDEX = 31; // index noting this is XZR

private:
    static std::string hex64(uint64_t v) {
        std::ostringstream ss;
        ss << "0x" << std::hex << std::setfill('0') << std::setw(16) << std::nouppercase << v;
        return ss.str();
    }

    std::array<uint64_t, 31> x_{};
    uint64_t sp_{0};
    uint64_t pc_{0};
    ProcessorState psr_{};
};

} // namespace arm64

#endif // ARM64_REGISTERS_HPP
