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
    static constexpr std::size_t kSize = 256;   // 256 bytes, per the task

    explicit Stack(uint64_t base_address = 0x0)
        : base_(base_address) {
        mem_.fill(0);
    }

    uint64_t base() const { return base_; }
    std::size_t size() const { return kSize; }

    // Optional helpers if you want to use it later
    void clear() { mem_.fill(0); }

    void write8(std::size_t offset, uint8_t v) {
        bounds_check(offset, 1);
        mem_[offset] = v;
    }
    uint8_t read8(std::size_t offset) const {
        bounds_check(offset, 1);
        return mem_[offset];
    }

    // (Convenience) Fill with random bytes (for demo of the “random” example)
    void fill_random(uint32_t seed = 0xC0FFEEu) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(0, 255);
        for (auto& b : mem_) b = static_cast<uint8_t>(dist(rng));
    }

    // Pretty-printer that matches the sample format exactly:
    //
    // -------------------------------------------------------------------------------------------------------------------------------Stack:
    //
    // -------------------------------------------------------------------------------------------------------------------------------00000000 <16 bytes> |<ascii>|
    // ...
    // 00000100
    //
    void print_dump(std::ostream& os) const {
        static constexpr const char* SEP =
            "-------------------------------------------------------------------------------------------------------------------------------";

        os << SEP << "\n"
           << "Stack:\n\n"
           << SEP;

        const std::size_t per_line = 16;
        for (std::size_t off = 0; off < kSize; off += per_line) {
            os << std::hex << std::nouppercase << std::setfill('0')
               << std::setw(8) << off << " ";

            // hex bytes (two lowercase hex digits + space)
            for (std::size_t i = 0; i < per_line; ++i) {
                os << std::setw(2) << static_cast<unsigned>(mem_[off + i]);
                if (i != per_line - 1) os << " ";
            }

            // ascii
            os << " |";
            for (std::size_t i = 0; i < per_line; ++i) {
                uint8_t c = mem_[off + i];
                if (c >= 0x20 && c <= 0x7e) os << static_cast<char>(c);
                else                        os << '.';
            }
            os << "|\n\n"; // blank line between rows (as in example)
        }

        // trailing end address line (0x00000100 for 256 bytes)
        os << std::hex << std::nouppercase << std::setfill('0')
           << std::setw(8) << kSize << "\n";
    }

private:
    void bounds_check(std::size_t offset, std::size_t width) const {
        if (offset + width > kSize) {
            throw std::out_of_range("stack write/read out of range");
        }
    }

    uint64_t base_;
    std::array<uint8_t, kSize> mem_{};
};

} // namespace arm64

#endif // ARM64_STACK_HPP