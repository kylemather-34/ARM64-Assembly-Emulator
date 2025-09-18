#ifndef ARM64_STACK_HPP
#define ARM64_STACK_HPP

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

namespace arm64 {

class Stack {
public:
    static constexpr std::size_t stackSize = 256;   // 256 bytes

    explicit Stack(uint64_t baseAddress = 0x0)
        : base_(baseAddress) {
        mem_.fill(0);
    }

    uint64_t base() const { return base_; }
    std::size_t size() const { return stackSize; }


    // Fill with random bytes for random example, potentially used later on for testing
    void fillRandom(uint32_t seed = 0xC0FFEEu) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(0, 255);
        for (auto& b : mem_) b = static_cast<uint8_t>(dist(rng));
    }

    // Dump the stack contents in a format similar to gdb’s x/256xb command
    // Prints in the same format as task sample output
    void printDump(std::ostream& os) const {
        static constexpr const char* SEP =
            "-------------------------------------------------------------------------------------------------------------------------------";

        os << SEP << "\n"
           << "Stack:\n\n"
           << SEP;

        const std::size_t bytesPerLine = 16;
        for (std::size_t off = 0; off < stackSize; off += bytesPerLine) {
            os << std::hex << std::nouppercase << std::setfill('0')
               << std::setw(8) << off << " ";

            // hex bytes
            for (std::size_t i = 0; i < bytesPerLine; i++) {
                os << std::setw(2) << static_cast<unsigned>(mem_[off + i]);
                if (i != bytesPerLine - 1) os << " ";
            }

            // ascii
            os << " |";
            for (std::size_t i = 0; i < bytesPerLine; i++) {
                uint8_t c = mem_[off + i];
                if (c >= 0x20 && c <= 0x7e) os << static_cast<char>(c);
                else                        os << '.';
            }
            os << "|\n\n"; // blank line between rows
        }

        // trailing end address line (0x00000100 for 256 bytes)
        os << std::hex << std::nouppercase << std::setfill('0')
           << std::setw(8) << stackSize << "\n";
    }

private:
    void boundsCheck(std::size_t offset, std::size_t width) const {
        if (offset + width > stackSize) {
            throw std::out_of_range("stack write/read out of range"); // Throws error for if memory accessed is out of bounds
        }
    }

    uint64_t base_;
    std::array<uint8_t, stackSize> mem_{};
};

} // namespace arm64

#endif // ARM64_STACK_HPP