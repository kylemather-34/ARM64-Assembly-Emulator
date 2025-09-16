/*
* ARM64 Assembly Parser
*
* This program reads an input text file containing ARM64 Assembly instructions
* and parses it to extract:
*   1. The instruction mnemonic and
*   2. The operands associated with the instruction (registers, memory references, labels, etc.)
*
* - Strips comments, labels and blank lines before parsing.
* - Splits operands even when inside memory brackets.
* - Classifies operands into types (Register, Immediate, Memory, Label).
* - Uses instruction-specific handlers for stricter parsing (ex. ADD, LDR, STR).
* - Extensible design, additional instruction handlers can be added.
*/

#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace arm64 {

// ---------- helpers ----------
static std::string trim(std::string s) {
    auto not_space = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

static bool is_register_token(const std::string& t) {
    if (t.empty()) return false;
    std::string u = upper(t);
    // crude: X0..X30, W0..W30, SP, XZR/WZR
    if (u == "SP" || u == "XZR" || u == "WZR") return true;
    if ((u[0] == 'X' || u[0] == 'W') && u.size() >= 2) {
        for (size_t i = 1; i < u.size(); ++i) if (!std::isdigit(static_cast<unsigned char>(u[i]))) return false;
        return true;
    }
    return false;
}

static bool is_immediate_token(const std::string& t) {
    return !t.empty() && t[0] == '#';
}

static bool is_memory_token(const std::string& t) {
    // [X0], [X1,#8], [SP], etc.
    return !t.empty() && t.front() == '[' && t.back() == ']';
}

static int64_t parse_immediate_value(const std::string& t) {
    // expects "#<value>"
    std::string v = t.substr(1);
    // allow 0x.. hex
    if (v.rfind("0x", 0) == 0 || v.rfind("0X", 0) == 0) {
        return static_cast<int64_t>(std::stoll(v, nullptr, 16));
    }
    return static_cast<int64_t>(std::stoll(v, nullptr, 10));
}

// FIXED: parse a single operand (previous version had a stray/misaligned closing brace)
static Operand parse_operand(std::string tok) {
    tok = trim(tok);
    if (tok.empty()) return {};

    if (is_memory_token(tok)) {
        return Operand{OperandType::Memory, tok, 0};
    }

    if (is_register_token(tok)) {
        return Operand{OperandType::Register, tok, 0};
    }

    if (is_immediate_token(tok)) {
        return Operand{OperandType::Immediate, tok, parse_immediate_value(tok)};
    }

    // fallback: treat as label/symbol
    return Operand{OperandType::Label, tok, 0};
}

// split operands on commas, respecting simple brackets (basic)
static std::vector<Operand> parse_operands(const std::string& s) {
    std::vector<Operand> out;
    std::string cur;
    int bracket = 0;
    for (char c : s) {
        if (c == '[') bracket++;
        if (c == ']') bracket = std::max(0, bracket - 1);
        if (c == ',' && bracket == 0) {
            out.push_back(parse_operand(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!trim(cur).empty()) out.push_back(parse_operand(cur));
    return out;
}

// ---------- handlers (now implemented) ----------
DecodedInstruction GenericHandler::parse(const std::string& mnemonic,
                                         const std::vector<Operand>& ops) const {
    // Accept anything; this is a permissive fallback
    return DecodedInstruction{upper(mnemonic), ops};
}

DecodedInstruction AddHandler::parse(const std::string& mnemonic,
                                     const std::vector<Operand>& ops) const {
    // ADD <Xd>, <Xn>, <Xm|#imm>
    if (ops.size() != 3) {
        throw std::runtime_error("ADD expects 3 operands");
    }
    if (ops[0].type != OperandType::Register || ops[1].type != OperandType::Register) {
        throw std::runtime_error("ADD first two operands must be registers");
    }
    if (ops[2].type != OperandType::Register && ops[2].type != OperandType::Immediate) {
        throw std::runtime_error("ADD third operand must be register or immediate");
    }
    return DecodedInstruction{upper(mnemonic), ops};
}

DecodedInstruction LdrHandler::parse(const std::string& mnemonic,
                                     const std::vector<Operand>& ops) const {
    // LDR <Xt>, [<Xn>{, #imm}]
    if (ops.size() < 2 || ops.size() > 2) {
        throw std::runtime_error("LDR expects 2 operands");
    }
    if (ops[0].type != OperandType::Register) {
        throw std::runtime_error("LDR destination must be a register");
    }
    if (ops[1].type != OperandType::Memory) {
        throw std::runtime_error("LDR address must be a memory operand like [Xn{,#imm}]");
    }
    return DecodedInstruction{upper(mnemonic), ops};
}

// ---------- Parser implementation (NEW) ----------
std::optional<DecodedInstruction> Parser::parse_line(const std::string& line) const {
    std::string s = trim(line);
    if (s.empty()) return std::nullopt;
    // comments starting with ';' or '//'
    if (s.rfind("//", 0) == 0 || s[0] == ';') return std::nullopt;

    // Extract mnemonic (first token) and rest (operands)
    std::string mnemonic;
    std::string rest;
    {
        std::istringstream iss(s);
        iss >> mnemonic;
        std::getline(iss, rest); // remainder including leading space before operands
        rest = trim(rest);
    }
    if (mnemonic.empty()) return std::nullopt;

    // parse operands
    auto ops = rest.empty() ? std::vector<Operand>{} : parse_operands(rest);

    // dispatch
    std::unique_ptr<InstructionHandler> handler;
    const std::string up = upper(mnemonic);
    if (up == "ADD") {
        handler = std::make_unique<AddHandler>();
    } else if (up == "LDR") {
        handler = std::make_unique<LdrHandler>();
    } else {
        handler = std::make_unique<GenericHandler>();
    }

    return handler->parse(mnemonic, ops);
}

// ---------- printing ----------
static std::string mem_arrow(const std::string& memText) {
    // Expect forms like: [SP], [SP, #8], [SP, 0x08], [X1,#16]
    // We’ll render: "[SP, 0x08] --> SP + 0x08"
    if (memText.size() < 2 || memText.front() != '[' || memText.back() != ']') {
        return memText; // not a bracketed memory form; bail
    }
    std::string inside = trim(std::string(memText.begin() + 1, memText.end() - 1));

    // split on first comma (if any)
    std::string base = inside;
    std::string off;
    auto comma = inside.find(',');
    if (comma != std::string::npos) {
        base = trim(inside.substr(0, comma));
        off  = trim(inside.substr(comma + 1));
        // drop an optional leading '#'
        if (!off.empty() && off.front() == '#') off.erase(off.begin());
    }

    if (off.empty()) {
        // just "[BASE]" — no arrow needed beyond echo
        return "[" + inside + "]";
    }
    return "[" + inside + "] --> " + base + " + " + off;
}

void print_decoded(std::size_t lineNo, const DecodedInstruction& inst) {
    // separator line (120 chars of '-')
    constexpr const char* SEP =
        "-------------------------------------------------------------------------------------------------------------------------------";

    std::cout << SEP << "\n";
    std::cout << "Instruction #" << lineNo << ":\n\n";
    std::cout << SEP << "\n\n";

    std::cout << "Instruction: " << inst.mnemonic << "\n\n";

    for (std::size_t i = 0; i < inst.operands.size(); ++i) {
        const auto& o = inst.operands[i];
        std::string out = o.text;
        if (o.type == OperandType::Memory) {
            out = mem_arrow(o.text);
        }
        std::cout << "Operand #" << (i + 1) << ": " << out << "\n\n";
    }
}

} // namespace arm64