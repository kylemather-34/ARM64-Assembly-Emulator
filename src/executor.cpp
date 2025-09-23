#include "executor.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace arm64 {

// String helper functions
static std::string trimCopy(std::string s) {
    auto not_space = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static std::string upperCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

// Label collection
static std::string collectLeadingLabels(std::string line, uint64_t next_instr_addr, std::unordered_map<std::string, uint64_t>& out) {
    std::string s = trimCopy(std::move(line));
    while (true) {
        auto pos = s.find(':');
        if (pos == std::string::npos) break;
        std::string left = trimCopy(s.substr(0, pos));
        std::string right = (pos + 1 < s.size()) ? trimCopy(s.substr(pos + 1)) : std::string{};
        if (left.empty()) break;
        if (left.find_first_of(" \t") != std::string::npos) break;
        out[upperCopy(left)] = next_instr_addr;
        s = right;
        if (s.empty()) break;
    }
    return s;
}

// Register helper functions
static bool isWReg(const std::string& tokU) { return !tokU.empty() && tokU[0] == 'W'; }
static bool isXReg(const std::string& tokU) { return !tokU.empty() && tokU[0] == 'X'; }

static unsigned regIndex(std::string tok) {
    std::string u = upperCopy(trimCopy(tok));
    if (u == "XZR" || u == "WZR") return Registers::XZR_INDEX;
    if (u == "SP") return Registers::XZR_INDEX + 1; // sentinel for SP
    if ((u[0] == 'X' || u[0] == 'W') && u.size() >= 2) {
        for (size_t i = 1; i < u.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(u[i]))) return 999;
        int n = std::stoi(u.substr(1));
        if (n >= 0 && n <= 30) return static_cast<unsigned>(n);
    }
    return 999;
}

// Decode memory operand to effective address
static uint64_t parseImm(std::string s) {
    s = trimCopy(s);
    if (!s.empty() && s[0] == '#') s.erase(s.begin());
    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
        return static_cast<uint64_t>(std::stoull(s, nullptr, 16));
    }
    return static_cast<uint64_t>(std::stoull(s, nullptr, 10));
}

static uint64_t effectiveAddr(const Operand& mem, const Registers& regs) {
    // mem.raw like "[SP, #8]" or "[X1, 0x10]" or "[SP]"
    const std::string& t = mem.raw;
    if (t.size() < 2 || t.front() != '[' || t.back() != ']') {
        throw std::runtime_error("invalid memory operand: " + t);
    }
    std::string inside = trimCopy(std::string(t.begin()+1, t.end()-1));
    std::string base = inside;
    std::string off;
    auto comma = inside.find(',');
    if (comma != std::string::npos) {
        base = trimCopy(inside.substr(0, comma));
        off  = trimCopy(inside.substr(comma + 1));
    }
    std::string bu = upperCopy(base);
    unsigned b = regIndex(bu);

    uint64_t base_val = 0;
    if (b == Registers::XZR_INDEX) {
        base_val = 0;
    } else if (b == Registers::XZR_INDEX + 1) { // SP
        base_val = regs.readSP();
    } else if (b != 999) {
        // assume Xn base (use 64-bit)
        base_val = regs.readX(b);
    } else {
        throw std::runtime_error("invalid base register in memory operand: " + base);
    }

    uint64_t off_val = 0;
    if (!off.empty()) off_val = parseImm(off);

    return base_val + off_val;
}

// Stack read and write capabilities for 64 bit
static void stackWrite64(Stack& st, uint64_t addr, uint64_t v) {
    if (addr < st.base() || addr + 8 > st.base() + st.size())
        throw std::runtime_error("STR out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    for (int i = 0; i < 8; ++i) st.write8(o + i, static_cast<uint8_t>((v >> (i*8)) & 0xFF));
}
static uint64_t stackRead64(const Stack& st, uint64_t addr) {
    if (addr < st.base() || addr + 8 > st.base() + st.size())
        throw std::runtime_error("LDR out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(st.read8(o + i)) << (i*8));
    return v;
}
static void stackWrite8(Stack& st, uint64_t addr, uint8_t v) {
    if (addr < st.base() || addr + 1 > st.base() + st.size())
        throw std::runtime_error("STRB out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    st.write8(o, v);
}
static uint8_t stackRead8(const Stack& st, uint64_t addr) {
    if (addr < st.base() || addr + 1 > st.base() + st.size())
        throw std::runtime_error("LDRB out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    return st.read8(o);
}

// Stack read and write capabilities for 32 bit
static void stackWrite32(Stack& st, uint64_t addr, uint32_t v) {
    if (addr < st.base() || addr + 4 > st.base() + st.size())
        throw std::runtime_error("STR (32) out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    for (int i = 0; i < 4; ++i) st.write8(o + i, static_cast<uint8_t>((v >> (i*8)) & 0xFF));
}

static uint32_t stackRead32(const Stack& st, uint64_t addr) {
    if (addr < st.base() || addr + 4 > st.base() + st.size())
        throw std::runtime_error("LDR (32) out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= (static_cast<uint32_t>(st.read8(o + i)) << (i*8));
    return v;
}


// flags from SUB result
static void stackSubFlags64(ProcessorState& ps, uint64_t a, uint64_t b, uint64_t res) {
    ps.N = (res >> 63) & 1;
    ps.Z = (res == 0);
    // C is NOT borrow for subtraction
    ps.C = (a >= b);
    // V: signed overflow on a - b
    const bool sa = (a >> 63) & 1;
    const bool sb = (b >> 63) & 1;
    const bool sr = (res >> 63) & 1;
    ps.V = (sa != sb) && (sr != sa);
}
static void stackSubFlags32(ProcessorState& ps, uint32_t a, uint32_t b, uint32_t res) {
    ps.N = (res >> 31) & 1;
    ps.Z = (res == 0);
    ps.C = (a >= b);
    const bool sa = (a >> 31) & 1;
    const bool sb = (b >> 31) & 1;
    const bool sr = (res >> 31) & 1;
    ps.V = (sa != sb) && (sr != sa);
}

// Build program from file
AsmProgram buildFileProgram(const std::string& path, const Parser& parser) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("could not open input file: " + path);

    AsmProgram prog;
    std::string line;
    std::size_t src_line = 0;
    std::size_t instrIndex = 0;

    while (std::getline(in, line)) {
        ++src_line;
        const uint64_t next_addr = static_cast<uint64_t>(instrIndex) * 4ull;

        std::string rest = collectLeadingLabels(line, next_addr, prog.labels);

        std::string s = trimCopy(rest);
        if (s.empty()) continue;
        if (s.rfind("//", 0) == 0 || s[0] == ';') continue;

        auto decoded = parser.parseLine(s);      // have shim for parseLine if needed
        if (!decoded) continue;

        AsmInst ai;
        ai.addr = next_addr;
        ai.instrIndex = ++instrIndex;
        ai.inst = std::move(*decoded);

        prog.addr2idx[ai.addr] = prog.code.size();
        prog.code.push_back(std::move(ai));
    }

    return prog;
}

// Execute an instruction, return true if more instructions remain
bool step(const AsmProgram& prog, Registers& regs, Stack& stack, uint64_t& pc) {
    if (prog.code.empty()) return false;
    const uint64_t endAddr = (prog.code.size() * 4ull);
    if (pc == endAddr) return false;

    auto it = prog.addr2idx.find(pc);
    if (it == prog.addr2idx.end()) {
        throw std::runtime_error("PC points to unknown address: " + std::to_string(pc));
    }
    const AsmInst& ai = prog.code[it->second];

    // Default next PC
    uint64_t nextPC = pc + 4ull;

    const std::string up = upperCopy(ai.inst.mnem);
    const auto& ops = ai.inst.operands;

    auto isImm = [&](size_t i){ return i < ops.size() && ops[i].type == OperandType::Immediate; };
    auto isReg = [&](size_t i){ return i < ops.size() && ops[i].type == OperandType::Register; };
    auto isMem = [&](size_t i){ return i < ops.size() && ops[i].type == OperandType::Memory; };

    auto regU = [&](size_t i){ return upperCopy(ops.at(i).raw); };

    auto readSrc64 = [&](size_t i)->uint64_t {
        // If token is Wn -> zero-extend; if Xn -> 64-bit
        std::string u = regU(i);
        unsigned r = regIndex(u);
        if (r == Registers::XZR_INDEX) return 0;
        if (r == Registers::XZR_INDEX + 1) return regs.readSP();
        if (r == 999) throw std::runtime_error("invalid register: " + ops[i].raw);
        if (isWReg(u)) return static_cast<uint64_t>(regs.readW(r));
        return regs.readX(r);
    };
    auto writeDest = [&](size_t i, uint64_t value){
        std::string u = regU(i);
        unsigned r = regIndex(u);
        if (r == Registers::XZR_INDEX) return;              // write ignored
        if (r == Registers::XZR_INDEX + 1) { regs.writeSP(value); return; }
        if (r == 999) throw std::runtime_error("invalid dest register");
        if (isWReg(u)) regs.writeW(r, static_cast<uint32_t>(value));
        else                   regs.writeX(r, value);
    };

    auto reamImm64 = [&](size_t i)->uint64_t {
        if (!isImm(i)) throw std::runtime_error("immediate expected");
        return static_cast<uint64_t>(ops[i].imm);
    };

    // execute
    if (up == "NOP") {
        // do nothing
    }
    else if (up == "MOV") {
        // MOV Rd, Rn | #imm
        if (ops.size() != 2) throw std::runtime_error("MOV expects 2 operands");
        uint64_t v = isImm(1) ? reamImm64(1) : readSrc64(1);
        writeDest(0, v);
    }
    else if (up == "ADD" || up == "SUB" || up == "AND" || up == "EOR" || up == "MUL") {
        if (ops.size() != 3) throw std::runtime_error(up + " expects 3 operands");
        uint64_t a = readSrc64(1);
        uint64_t b = isImm(2) ? reamImm64(2) : readSrc64(2);

        uint64_t res = 0;
        if (up == "ADD")      res = a + b;
        else if (up == "SUB") res = a - b;
        else if (up == "AND") res = (a & b);
        else if (up == "EOR") res = (a ^ b);
        else                  res = (a * b); // MUL (low 64)
        writeDest(0, res);
        // (No flags unless you add *S variants; CMP handles flags)
    }
    else if (up == "CMP") {
        // CMP Rn, Rm | #imm    (sets N,Z,C,V like SUBS)
        if (ops.size() != 2 || !isReg(0))
            throw std::runtime_error("CMP expects Rn, (Rm|#imm)");
        std::string rnU = regU(0);
        uint64_t a = readSrc64(0);
        uint64_t b = isImm(1) ? reamImm64(1) : readSrc64(1);

        if (isWReg(rnU)) {
            uint32_t aa = static_cast<uint32_t>(a);
            uint32_t bb = static_cast<uint32_t>(b);
            uint32_t rr = static_cast<uint32_t>(aa - bb);
            stackSubFlags32(regs.state(), aa, bb, rr);
        } else {
            uint64_t rr = a - b;
            stackSubFlags64(regs.state(), a, b, rr);
        }
    }
    else if (up == "LDR" || up == "LDRB") {
        // LDR Rt, [base{,#off}]   /   LDRB Rt, [base{,#off}]
        if (ops.size() != 2 || !isReg(0) || !isMem(1))
            throw std::runtime_error(up + " expects Rt, [base{,#off}]");

        uint64_t ea = effectiveAddr(ops[1], regs);

        if (up == "LDRB") {
            uint8_t byte = stackRead8(stack, ea);
            // write byte zero-extended into dest width
            std::string rtU = regU(0);
            if (isWReg(rtU)) writeDest(0, static_cast<uint32_t>(byte));
            else                      writeDest(0, static_cast<uint64_t>(byte));
        } else { // LDR
            std::string rtU = regU(0);
            if (isWReg(rtU)) {
                uint32_t w = stackRead32(stack, ea);         // 4 bytes, zero-extend
                writeDest(0, static_cast<uint64_t>(w));
            } else {
                uint64_t x = stackRead64(stack, ea);         // 8 bytes
                writeDest(0, x);
            }
        }
    }
    else if (up == "STR" || up == "STRB") {
        if (ops.size() != 2 || !isReg(0) || !isMem(1))
            throw std::runtime_error(up + " expects Rt, [base{,#off}]");

        uint64_t ea = effectiveAddr(ops[1], regs);
        std::string rtU = regU(0);

        if (up == "STRB") {
            uint64_t v = readSrc64(0);
            stackWrite8(stack, ea, static_cast<uint8_t>(v & 0xFF));
        } else { // STR
            if (isWReg(rtU)) {
                uint32_t w = static_cast<uint32_t>(readSrc64(0));  // low 32
                stackWrite32(stack, ea, w);                         // 4 bytes
            } else {
                uint64_t x = readSrc64(0);
                stackWrite64(stack, ea, x);                         // 8 bytes
            }
        }
    }

    else if (up == "B") {
        if (ops.size() != 1 || ops[0].type != OperandType::Label)
            throw std::runtime_error("B expects a single label operand");
        std::string key = upperCopy(trimCopy(ops[0].raw));
        auto jt = prog.labels.find(key);
        if (jt == prog.labels.end()) throw std::runtime_error("undefined label: " + ops[0].raw);
        nextPC = jt->second;
    }
    else if (up == "B.GT" || up == "B.LE") {
        if (ops.size() != 1 || ops[0].type != OperandType::Label)
            throw std::runtime_error(up + " expects a single label operand");
        const auto& ps = regs.state();
        bool take = false;
        if (up == "B.GT") take = (!ps.Z && (ps.N == ps.V));
        else              take = ( ps.Z || (ps.N != ps.V));

        if (take) {
            std::string key = upperCopy(trimCopy(ops[0].raw));
            auto jt = prog.labels.find(key);
            if (jt == prog.labels.end()) throw std::runtime_error("undefined label: " + ops[0].raw);
            nextPC = jt->second;
        }
    }
    else if (up == "RET") {
        return false; // halt emulation
    }
    else {
        // Unimplemented mnemonic, treat as NOP or throw:
        // throw std::runtime_error("unimplemented instruction: " + up);
    }

    pc = nextPC;
    regs.writePC(pc);
    return (pc != endAddr);
}

} // namespace arm64