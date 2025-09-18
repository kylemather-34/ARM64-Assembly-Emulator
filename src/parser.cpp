#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace arm64 {

/* Helper functions for formatting strings and parsing tokens */

// trim whitespace from both ends
static std::string trim(std::string s) {
    auto not_space = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

// uppercase
static std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

/* Simple checks for token types */

// Is this token a register?
static bool isReg(const std::string& t) {
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

// Is this token an immediate value?
static bool isImmediate(const std::string& t) {
    return !t.empty() && t[0] == '#';
}

// Is this token a memory reference?
static bool isMem(const std::string& t) {
    // [X0], [X1,#8], [SP], etc.
    return !t.empty() && t.front() == '[' && t.back() == ']';
}

// Parse immediate value from token if applicable
static int64_t parseImmediate(const std::string& t) {
    std::string v = t.substr(1);
    if (v.rfind("0x", 0) == 0 || v.rfind("0X", 0) == 0) {
        return static_cast<int64_t>(std::stoll(v, nullptr, 16));
    }
    return static_cast<int64_t>(std::stoll(v, nullptr, 10));
}

// Parse a single operand
static Operand parseOp(std::string tok) {
    tok = trim(tok);
    if (tok.empty()) return {};

    if (isMem(tok)) {
        return Operand{OperandType::Memory, tok};
    }

    if (isReg(tok)) {
        return Operand{OperandType::Register, tok};
    }

    if (isImmediate(tok)) {
        return Operand{OperandType::Immediate, tok, parseImmediate(tok)};
    }

    // Treat as label or symbol if necessary
    return Operand{OperandType::Label, tok, 0};
}

// Split operands on commas with respect to brackets
static std::vector<Operand> parseOps(const std::string& s) {
    std::vector<Operand> out;
    std::string cur;
    int bracket = 0;
    for (char c : s) {
        if (c == '[') bracket++;
        if (c == ']') bracket = std::max(0, bracket - 1);
        if (c == ',' && bracket == 0) {
            out.push_back(parseOp(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!trim(cur).empty()) out.push_back(parseOp(cur));
    return out;
}

// Handlers for specific instructions from include/parser.hpp now implemented
DecodedInstruction GenericHandler::parse(const std::string& mnem, const std::vector<Operand>& ops) const {
    return DecodedInstruction{upper(mnem), ops};
}

DecodedInstruction AddHandler::parse(const std::string& mnem, const std::vector<Operand>& ops) const {
    if (ops.size() != 3) {
        throw std::runtime_error("ADD expects 3 operands");
    }
    if (ops[0].type != OperandType::Register || ops[1].type != OperandType::Register) {
        throw std::runtime_error("ADD first two operands must be registers");
    }
    if (ops[2].type != OperandType::Register && ops[2].type != OperandType::Immediate) {
        throw std::runtime_error("ADD third operand must be register or immediate");
    }
    return DecodedInstruction{upper(mnem), ops};
}

DecodedInstruction LdrHandler::parse(const std::string& mnem, const std::vector<Operand>& ops) const {
    if (ops.size() < 2 || ops.size() > 2) {
        throw std::runtime_error("LDR expects 2 operands");
    }
    if (ops[0].type != OperandType::Register) {
        throw std::runtime_error("LDR destination must be a register");
    }
    if (ops[1].type != OperandType::Memory) {
        throw std::runtime_error("LDR address must be a memory operand like [Xn{,#imm}]");
    }
    return DecodedInstruction{upper(mnem), ops};
}

// Implemented Parser class methods
std::optional<DecodedInstruction> Parser::parseLine(const std::string& line) const {
    std::string s = trim(line);
    if (s.empty()) return std::nullopt;
    // comments starting with ';' or '//'
    if (s.rfind("//", 0) == 0 || s[0] == ';') return std::nullopt;

    // Extract mnemonic (which is first token) and rest of instruction (operands)
    std::string mnem;
    std::string rest;
    {
        std::istringstream iss(s);
        iss >> mnem;
        std::getline(iss, rest); // remainder including leading space before operands
        rest = trim(rest);
    }
    if (mnem.empty()) return std::nullopt;

    // parse operands
    auto ops = rest.empty() ? std::vector<Operand>{} : parseOps(rest);

    std::unique_ptr<InstructionHandler> handler;
    const std::string up = upper(mnem);
    if (up == "ADD") {
        handler = std::make_unique<AddHandler>();
    } else if (up == "LDR") {
        handler = std::make_unique<LdrHandler>();
    } else {
        handler = std::make_unique<GenericHandler>();
    }

    return handler->parse(mnem, ops);
}

// Printing, matches sample output in Task 1
static std::string memArrow(const std::string& memRaw) {
    if (memRaw.size() < 2 || memRaw.front() != '[' || memRaw.back() != ']') {
        return memRaw; // if not a bracketed memory form then exit
    }
    std::string inside = trim(std::string(memRaw.begin() + 1, memRaw.end() - 1));

    // split on first comma (if any exist)
    std::string base = inside;
    std::string off;
    auto comma = inside.find(',');
    if (comma != std::string::npos) {
        base = trim(inside.substr(0, comma));
        off  = trim(inside.substr(comma + 1));
        if (!off.empty() && off.front() == '#') off.erase(off.begin());
    }

    if (off.empty()) {
        return "[" + inside + "]";
    }
    return "[" + inside + "] --> " + base + " + " + off;
}

void printDecoded(std::size_t lineNo, const DecodedInstruction& inst) {
    // separator line as shown in sample output
    constexpr const char* SEP =
        "-------------------------------------------------------------------------------------------------------------------------------";

    std::cout << SEP << "\n";
    std::cout << "Instruction #" << lineNo << ":\n\n";
    std::cout << SEP << "\n\n";

    std::cout << "Instruction: " << inst.mnem << "\n\n";

    for (std::size_t i = 0; i < inst.operands.size(); ++i) {
        const auto& o = inst.operands[i];
        std::string out = o.raw;
        if (o.type == OperandType::Memory) {
            out = memArrow(o.raw);
        }
        std::cout << "Operand #" << (i + 1) << ": " << out << "\n\n";
    }
}

} // namespace arm64