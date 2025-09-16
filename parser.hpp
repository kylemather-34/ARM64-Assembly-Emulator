#ifndef ARM64_PARSER_HPP
#define ARM64_PARSER_HPP


#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>


/*
* ARM64 Assembly Parser (Header)
* ------------------------------
* Public types and parsing interface for the ARM64 assembly parser.
* See src/arm64_parser.cpp for implementation details.
*/


namespace arm64 {


// ----------------------------- Utilities -----------------------------
std::string ltrim(std::string s);
std::string rtrim(std::string s);
std::string trim(std::string s);
std::string upper(std::string s);
std::vector<std::string> split_operands(const std::string &s);


// ----------------------------- Operand model -----------------------------
enum class OperandType { Register, Immediate, Memory, Label, Unknown };


struct Operand {
OperandType type = OperandType::Unknown;
std::string text; // raw token (trimmed) e.g., "X1", "#42", "[SP, 0x8]"


// decoded helpers
std::string reg; // X0..X30, W0..W30, SP, FP, LR, XZR/WZR
long long imm = 0; // decoded immediate value


// memory addressing
std::string memBase; // e.g., SP, FP
long long memOffset = 0;
bool memHasOffset = false;


std::string pretty() const; // e.g., "[SP, 0x08] --> SP + 0x08"
};


Operand parse_operand(std::string tok);


// ----------------------------- Instruction model -----------------------------
struct DecodedInstruction{
std::string mnemonic; // e.g., ADD
std::vector<Operand> operands; // parsed operands
};


// Base class for instruction-specific parsers
struct IHandler{
virtual ~IHandler() = default;
virtual DecodedInstruction parse(const std::string& mnemonic, const std::string &operandStr) const = 0;
};


// Generic split-and-parse handler with optional arity checks
struct GenericHandler final : IHandler{
size_t minOps, maxOps; // 0 means unbounded for that side
explicit GenericHandler(size_t minOps=0, size_t maxOps=0): minOps(minOps), maxOps(maxOps) {}
DecodedInstruction parse(const std::string& mnemonic, const std::string &operandStr) const override;
};


// Specific example handlers
struct AddHandler final : IHandler{ // ADD/SUB register form (basic)
DecodedInstruction parse(const std::string& mnemonic, const std::string &operandStr) const override;
};


struct LdrHandler final : IHandler{ // LDR Rt, [BASE{, #imm}]
DecodedInstruction parse(const std::string& mnemonic, const std::string &operandStr) const override;
};
#endif // ARM64_PARSER_HPP

}