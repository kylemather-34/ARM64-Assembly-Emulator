#ifndef ARM64_PARSER_HPP
#define ARM64_PARSER_HPP

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <iostream>

namespace arm64 {

// --- Existing types (keep yours; below are minimal versions if they were missing) ---
enum class OperandType { Register, Immediate, Memory, Label };

struct Operand {
    OperandType type{};
    std::string text;     // raw token (e.g., "X0", "#42", "[X1]")
    int64_t     imm{0};   // optional parsed immediate
};

struct DecodedInstruction {
    std::string mnemonic;
    std::vector<Operand> operands;
};

// Base handler
struct InstructionHandler {
    virtual ~InstructionHandler() = default;
    virtual DecodedInstruction parse(const std::string& mnemonic,
                                     const std::vector<Operand>& ops) const = 0;
};

// Concrete handlers (previously only declared; now kept declared here and defined in .cpp)
struct GenericHandler final : InstructionHandler {
    DecodedInstruction parse(const std::string& mnemonic,
                             const std::vector<Operand>& ops) const override;
};

struct AddHandler final : InstructionHandler {
    DecodedInstruction parse(const std::string& mnemonic,
                             const std::vector<Operand>& ops) const override;
};

struct LdrHandler final : InstructionHandler {
    DecodedInstruction parse(const std::string& mnemonic,
                             const std::vector<Operand>& ops) const override;
};

// NEW: Parser class that main.cpp uses
class Parser {
public:
    // Parse a single assembly line; returns std::nullopt for blank/comment lines
    std::optional<DecodedInstruction> parse_line(const std::string& line) const;
};

// NEW: print utility that main.cpp calls
void print_decoded(std::size_t lineNo, const DecodedInstruction& inst);

} // namespace arm64

#endif // ARM64_PARSER_HPP