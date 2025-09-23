/*
* ARM64 Emulator Executor
*
* This header declares the interfaces for building a linear program
* from parsed assembly and for single-step instruction emulation.
*
* - Assigns sequential addresses (0x0, 0x4, ...) to instructions and records labels.
* - Defines AsmProgram / AsmInst containers used by the emulator.
* - Exposes buildFileProgram(...) and step(...).
* - Executes the Task-5 instruction set:
*     ADD, SUB, AND, EOR, MUL, MOV,
*     STR, STRB, LDR, LDRB,
*     CMP, B, B.GT, B.LE, NOP, RET.
* - Updates PC, general-purpose registers, and processor state flags as needed.
* - Enforces 32-/64-bit semantics: Wn reads/writes low 32-bits (zero-extend on
*   destination), Xn operate on full 64-bits.
* - Uses the 256-byte stack model for all memory accesses with bounds checks.
*
* Author: Kyle Mather and Braeden Allen
*/

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
    uint64_t addr;
    std::size_t instrIndex; // 1-based index of instruction in program
    DecodedInstruction inst;
};

struct AsmProgram {
    std::vector<AsmInst> code;
    std::unordered_map<std::string, uint64_t> labels;
    std::unordered_map<uint64_t, std::size_t> addr2idx;
};

// First pass: parse and assign addresses; collect labels.
AsmProgram buildFileProgram(const std::string& path, const Parser& parser);

// Execute a single instruction at PC -> updates regs/stack/PC.
// Returns false to halt (RET) or when PC == end.
bool step(const AsmProgram& prog, Registers& regs, Stack& stack, uint64_t& pc);

} // namespace arm64

#endif // ARM64_EXECUTOR_HPP
