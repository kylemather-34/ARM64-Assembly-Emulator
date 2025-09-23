/*
* ARM64 Assembly Parser
*
* This program reads an input raw file containing ARM64 Assembly instructions
* and parses it to extract: The instruction mnemonic, and the operands associated 
* with the instruction (registers, memory references, labels, etc.)
*
* - Strips comments, labels and blank lines before parsing.
* - Splits operands even when inside memory brackets.
* - Classifies operands into types (Register, Immediate, Memory, Label).
* - Uses instruction-specific handlers for stricter parsing (ex. ADD, LDR, STR).
* - Extensible design, additional instruction handlers can be added.

* Author: Kyle Mather and Braeden Allen
*/

#ifndef ARM64_PARSER_HPP
#define ARM64_PARSER_HPP

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <iostream>

namespace arm64 {

// Operand types
enum class OperandType { Register, Immediate, Memory, Label };

struct Operand {
    OperandType type{};
    std::string raw; // raw token
    int64_t     imm{0}; // immediate value if applicable
};

struct DecodedInstruction {
    std::string mnem;
    std::vector<Operand> operands;
};

// Base handler
struct InstructionHandler {
    virtual ~InstructionHandler() = default;
    virtual DecodedInstruction parse(const std::string& mnem, const std::vector<Operand>& ops) const = 0;
};

// Concrete handlers for different instruction types
struct GenericHandler final : InstructionHandler {
    DecodedInstruction parse(const std::string& mnem, const std::vector<Operand>& ops) const override;
};

struct AddHandler final : InstructionHandler {
    DecodedInstruction parse(const std::string& mnem, const std::vector<Operand>& ops) const override;
};

struct LdrHandler final : InstructionHandler {
    DecodedInstruction parse(const std::string& mnem, const std::vector<Operand>& ops) const override;
};

// Parser class used in src/parser_main.cpp
class Parser {
public:
    // Parse a single line of assembly
    std::optional<DecodedInstruction> parseLine(const std::string& line) const;
};

// Print function called in src/parser_main.cpp
void printDecoded(std::size_t lineNo, const DecodedInstruction& inst);

} // namespace arm64

#endif // ARM64_PARSER_HPP