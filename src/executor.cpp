#include "executor.hpp"
#include "parser.hpp"
#include "registers.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace arm64 {

// local helpers (do not clash with parser.cpp statics)
static std::string trim_copy(std::string s) {
    auto not_space = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static std::string upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static bool is_comment_or_blank(const std::string& s) {
    if (s.empty()) return true;
    if (s.rfind("//", 0) == 0 || s[0] == ';') return true;
    // Treat bare-label lines as not an instruction (e.g., "loop:")
    if (s.back() == ':' && s.find_first_of(" \t") == std::string::npos) return false;
    return false;
}

// Extract leading labels like "loop:" or "A: B: label:"; returns residual line after labels.
static std::string collect_leading_labels(std::string line, uint64_t next_instr_addr,
                                          std::unordered_map<std::string, uint64_t>& out) {
    std::string s = trim_copy(std::move(line));
    while (true) {
        auto pos = s.find(':');
        if (pos == std::string::npos) break;
        // ensure it's a simple "label:"
        std::string left = trim_copy(s.substr(0, pos));
        std::string right = (pos + 1 < s.size()) ? trim_copy(s.substr(pos + 1)) : std::string{};
        if (left.empty()) break;
        // Only accept if there are no spaces in the label token (simple symbol)
        if (left.find_first_of(" \t") != std::string::npos) break;

        out[upper_copy(left)] = next_instr_addr; // labels point at next instruction's address
        s = right;
        if (s.empty()) break; // bare label line; done
    }
    return s;
}

// parse register token "Xn" / "Wn" / "SP" / "XZR"/"WZR" -> 0..31 (31 = ZR)
static unsigned reg_index_from_token(std::string tok) {
    std::string u = upper_copy(trim_copy(tok));
    if (u == "XZR" || u == "WZR") return Registers::XZR_INDEX;
    if (u == "SP") return Registers::XZR_INDEX + 1; // sentinel to mark SP (not valid Rt for CBZ/CBNZ)
    if ((u[0] == 'X' || u[0] == 'W') && u.size() >= 2) {
        for (size_t i = 1; i < u.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(u[i]))) return 999;
        int n = std::stoi(u.substr(1));
        if (n >= 0 && n <= 30) return static_cast<unsigned>(n);
    }
    return 999; // invalid
}

AsmProgram build_program_from_file(const std::string& path, const Parser& parser) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("could not open input file: " + path);
    }

    AsmProgram prog;
    std::string line;
    std::size_t src_line = 0;
    std::size_t instr_index = 0; // count only real instructions

    while (std::getline(in, line)) {
        ++src_line;

        // `addr` for *next* instruction
        const uint64_t next_addr = static_cast<uint64_t>(instr_index) * 4ull;

        // collect zero or more leading labels
        std::string rest = collect_leading_labels(line, next_addr, prog.labels);

        // parse the rest as an instruction (if any)
        std::string s = trim_copy(rest);
        if (s.empty()) continue;
        if (s.rfind("//", 0) == 0 || s[0] == ';') continue;

        auto decoded = parser.parseLine(s);
        if (!decoded) continue; // comment/blank as judged by the parser

        // store instruction with its address
        AsmInst ai;
        ai.addr = next_addr;
        ai.instr_index = ++instr_index; // 1-based
        ai.inst = std::move(*decoded);
        prog.addr2idx[ai.addr] = prog.code.size();
        prog.code.push_back(std::move(ai));
    }

    return prog;
}

bool step(const AsmProgram& prog, Registers& regs, uint64_t& pc) {
    if (prog.code.empty()) return false;

    // program end when PC == (last_addr + 4)
    const uint64_t end_addr = (prog.code.size() * 4ull);
    if (pc == end_addr) return false;

    auto it = prog.addr2idx.find(pc);
    if (it == prog.addr2idx.end()) {
        // If PC jumped outside known code (but not exactly end), treat as error
        throw std::runtime_error("PC points to unknown address: 0x" + std::to_string(pc));
    }

    const AsmInst& ai = prog.code[it->second];

    // default: sequential
    uint64_t next_pc = pc + 4ull;

    const std::string up = upper_copy(ai.inst.mnem);

    auto lookup_label = [&](const std::string& labtok) -> uint64_t {
        std::string key = upper_copy(trim_copy(labtok));
        auto jt = prog.labels.find(key);
        if (jt == prog.labels.end()) {
            throw std::runtime_error("undefined label: " + labtok);
        }
        return jt->second;
    };

    if (up == "B") {
        // B <label>
        if (ai.inst.operands.size() != 1 || ai.inst.operands[0].type != OperandType::Label) {
            throw std::runtime_error("B expects a single label operand");
        }
        next_pc = lookup_label(ai.inst.operands[0].raw);
    } else if (up == "CBZ" || up == "CBNZ") {
        // CBZ <Rt>, <label>   /   CBNZ <Rt>, <label>
        if (ai.inst.operands.size() != 2 ||
            ai.inst.operands[0].type != OperandType::Register ||
            ai.inst.operands[1].type != OperandType::Label) {
            throw std::runtime_error(up + " expects: <Rt>, <label>");
        }

        const unsigned r = reg_index_from_token(ai.inst.operands[0].raw);
        if (r == Registers::XZR_INDEX + 1 || r == 999) {
            throw std::runtime_error(up + " first operand must be Xn/Wn (not SP)");
        }

        uint64_t value = (ai.inst.operands[0].raw.size() && std::toupper(ai.inst.operands[0].raw[0]) == 'W')
                         ? regs.readW(r)
                         : regs.readX(r);

        // For this simplified model, also reflect Z based on the test
        regs.state().Z = (value == 0);

        const bool take = (up == "CBZ") ? (value == 0) : (value != 0);
        if (take) {
            next_pc = lookup_label(ai.inst.operands[1].raw);
        }
    } else {
        // other instructions: nothing to do for PC
    }

    pc = next_pc;
    regs.writePC(pc);
    return (pc != end_addr);
}

} // namespace arm64