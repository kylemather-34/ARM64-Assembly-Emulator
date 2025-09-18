#ifndef ARM64_EXECUTOR_HPP
#define ARM64_EXECUTOR_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>

#include "parser.hpp"
#include "registers.hpp"
#include "stack.hpp"

namespace arm64 {

struct AsmInst {
    uint64_t addr;                // 0x0, 0x4, 0x8, ...
    std::size_t instr_index;      // 1-based index for pretty printing
    DecodedInstruction inst;
};

struct AsmProgram {
    std::vector<AsmInst> code;
    std::unordered_map<std::string, uint64_t> labels;   // UPPER(label) -> addr
    std::unordered_map<uint64_t, std::size_t> addr2idx; // addr -> code index
};

// First pass: parse and assign addresses; collect labels.
AsmProgram build_program_from_file(const std::string& path, const Parser& parser);

// Execute a single instruction at PC -> updates regs/stack/PC.
// Returns false to halt (RET) or when PC == end.
bool step(const AsmProgram& prog, Registers& regs, Stack& stack, uint64_t& pc);

} // namespace arm64

#endif // ARM64_EXECUTOR_HPP
