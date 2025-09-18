#ifndef ARM64_EXECUTOR_HPP
#define ARM64_EXECUTOR_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>

#include "parser.hpp"
#include "registers.hpp"

namespace arm64 {

struct AsmInst {
    uint64_t addr;                // instruction address (0x0, 0x4, ...)
    std::size_t instr_index;      // 1-based instruction index (for pretty printing)
    DecodedInstruction inst;
};

struct AsmProgram {
    std::vector<AsmInst> code;                           // linear code
    std::unordered_map<std::string, uint64_t> labels;   // UPPER(label) -> addr
    std::unordered_map<uint64_t, std::size_t> addr2idx; // addr -> index into code
};

// First pass: collect labels & instructions (with addresses)
AsmProgram build_program_from_file(const std::string& path, const Parser& parser);

// Execute one step starting from PC. Returns false when program has finished.
// Updates regs.PC appropriately (sequential + branching).
bool step(const AsmProgram& prog, Registers& regs, uint64_t& pc);

} // namespace arm64

#endif // ARM64_EXECUTOR_HPP
