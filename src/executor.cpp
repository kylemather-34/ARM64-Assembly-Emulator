#include "executor.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace arm64 {

// ---------- small string helpers ----------
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

// ---------- label collection (same idea as Task 4) ----------
static std::string collect_leading_labels(std::string line, uint64_t next_instr_addr,
                                          std::unordered_map<std::string, uint64_t>& out) {
    std::string s = trim_copy(std::move(line));
    while (true) {
        auto pos = s.find(':');
        if (pos == std::string::npos) break;
        std::string left = trim_copy(s.substr(0, pos));
        std::string right = (pos + 1 < s.size()) ? trim_copy(s.substr(pos + 1)) : std::string{};
        if (left.empty()) break;
        if (left.find_first_of(" \t") != std::string::npos) break;
        out[upper_copy(left)] = next_instr_addr;
        s = right;
        if (s.empty()) break;
    }
    return s;
}

// ---------- reg helpers ----------
static bool is_w_reg_token(const std::string& tokU) { return !tokU.empty() && tokU[0] == 'W'; }
static bool is_x_reg_token(const std::string& tokU) { return !tokU.empty() && tokU[0] == 'X'; }

static unsigned reg_index_from_token(std::string tok) {
    std::string u = upper_copy(trim_copy(tok));
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

// ---------- memory decode: [SP], [SP, #imm], [Xn, #imm], [Xn, 0x..] ----------
static uint64_t parse_imm_any(std::string s) {
    s = trim_copy(s);
    if (!s.empty() && s[0] == '#') s.erase(s.begin());
    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
        return static_cast<uint64_t>(std::stoull(s, nullptr, 16));
    }
    return static_cast<uint64_t>(std::stoull(s, nullptr, 10));
}

static uint64_t eff_addr_from_mem_token(const Operand& mem, const Registers& regs) {
    // mem.raw like "[SP, #8]" or "[X1, 0x10]" or "[SP]"
    const std::string& t = mem.raw;
    if (t.size() < 2 || t.front() != '[' || t.back() != ']') {
        throw std::runtime_error("invalid memory operand: " + t);
    }
    std::string inside = trim_copy(std::string(t.begin()+1, t.end()-1));
    std::string base = inside;
    std::string off;
    auto comma = inside.find(',');
    if (comma != std::string::npos) {
        base = trim_copy(inside.substr(0, comma));
        off  = trim_copy(inside.substr(comma + 1));
    }
    std::string bu = upper_copy(base);
    unsigned b = reg_index_from_token(bu);

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
    if (!off.empty()) off_val = parse_imm_any(off);

    return base_val + off_val;
}

// ---------- stack read/write ----------
static void stack_write64(Stack& st, uint64_t addr, uint64_t v) {
    if (addr < st.base() || addr + 8 > st.base() + st.size())
        throw std::runtime_error("STR out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    for (int i = 0; i < 8; ++i) st.write8(o + i, static_cast<uint8_t>((v >> (i*8)) & 0xFF));
}
static uint64_t stack_read64(const Stack& st, uint64_t addr) {
    if (addr < st.base() || addr + 8 > st.base() + st.size())
        throw std::runtime_error("LDR out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(st.read8(o + i)) << (i*8));
    return v;
}
static void stack_write8(Stack& st, uint64_t addr, uint8_t v) {
    if (addr < st.base() || addr + 1 > st.base() + st.size())
        throw std::runtime_error("STRB out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    st.write8(o, v);
}
static uint8_t stack_read8(const Stack& st, uint64_t addr) {
    if (addr < st.base() || addr + 1 > st.base() + st.size())
        throw std::runtime_error("LDRB out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    return st.read8(o);
}

// ---------- stack read/write 32-bit ----------
static void stack_write32(Stack& st, uint64_t addr, uint32_t v) {
    if (addr < st.base() || addr + 4 > st.base() + st.size())
        throw std::runtime_error("STR (32) out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    for (int i = 0; i < 4; ++i) st.write8(o + i, static_cast<uint8_t>((v >> (i*8)) & 0xFF));
}

static uint32_t stack_read32(const Stack& st, uint64_t addr) {
    if (addr < st.base() || addr + 4 > st.base() + st.size())
        throw std::runtime_error("LDR (32) out of stack bounds");
    std::size_t o = static_cast<std::size_t>(addr - st.base());
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= (static_cast<uint32_t>(st.read8(o + i)) << (i*8));
    return v;
}


// ---------- flags from SUB result (width 32/64) ----------
static void set_flags_sub_64(ProcessorState& ps, uint64_t a, uint64_t b, uint64_t res) {
    ps.N = (res >> 63) & 1;
    ps.Z = (res == 0);
    // C is NOT borrow for subtraction: a >= b (unsigned)
    ps.C = (a >= b);
    // V: signed overflow on a - b
    const bool sa = (a >> 63) & 1;
    const bool sb = (b >> 63) & 1;
    const bool sr = (res >> 63) & 1;
    ps.V = (sa != sb) && (sr != sa);
}
static void set_flags_sub_32(ProcessorState& ps, uint32_t a, uint32_t b, uint32_t res) {
    ps.N = (res >> 31) & 1;
    ps.Z = (res == 0);
    ps.C = (a >= b);
    const bool sa = (a >> 31) & 1;
    const bool sb = (b >> 31) & 1;
    const bool sr = (res >> 31) & 1;
    ps.V = (sa != sb) && (sr != sa);
}

// ---------- program build (same approach as Task 4) ----------
AsmProgram build_program_from_file(const std::string& path, const Parser& parser) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("could not open input file: " + path);

    AsmProgram prog;
    std::string line;
    std::size_t src_line = 0;
    std::size_t instr_index = 0;

    while (std::getline(in, line)) {
        ++src_line;
        const uint64_t next_addr = static_cast<uint64_t>(instr_index) * 4ull;

        std::string rest = collect_leading_labels(line, next_addr, prog.labels);

        std::string s = trim_copy(rest);
        if (s.empty()) continue;
        if (s.rfind("//", 0) == 0 || s[0] == ';') continue;

        auto decoded = parser.parseLine(s);      // have shim for parseLine if needed
        if (!decoded) continue;

        AsmInst ai;
        ai.addr = next_addr;
        ai.instr_index = ++instr_index;
        ai.inst = std::move(*decoded);

        prog.addr2idx[ai.addr] = prog.code.size();
        prog.code.push_back(std::move(ai));
    }

    return prog;
}

// ---------- execute one instruction ----------
bool step(const AsmProgram& prog, Registers& regs, Stack& stack, uint64_t& pc) {
    if (prog.code.empty()) return false;
    const uint64_t end_addr = (prog.code.size() * 4ull);
    if (pc == end_addr) return false;

    auto it = prog.addr2idx.find(pc);
    if (it == prog.addr2idx.end()) {
        throw std::runtime_error("PC points to unknown address: " + std::to_string(pc));
    }
    const AsmInst& ai = prog.code[it->second];

    // Default next PC (sequential)
    uint64_t next_pc = pc + 4ull;

    const std::string up = upper_copy(ai.inst.mnem);
    const auto& ops = ai.inst.operands;

    auto operand_is_imm = [&](size_t i){ return i < ops.size() && ops[i].type == OperandType::Immediate; };
    auto operand_is_reg = [&](size_t i){ return i < ops.size() && ops[i].type == OperandType::Register; };
    auto operand_is_mem = [&](size_t i){ return i < ops.size() && ops[i].type == OperandType::Memory; };

    auto regU = [&](size_t i){ return upper_copy(ops.at(i).raw); };

    auto read_src64 = [&](size_t i)->uint64_t {
        // If token is Wn -> zero-extend; if Xn -> 64-bit
        std::string u = regU(i);
        unsigned r = reg_index_from_token(u);
        if (r == Registers::XZR_INDEX) return 0;
        if (r == Registers::XZR_INDEX + 1) return regs.readSP();
        if (r == 999) throw std::runtime_error("invalid register: " + ops[i].raw);
        if (is_w_reg_token(u)) return static_cast<uint64_t>(regs.readW(r));
        return regs.readX(r);
    };
    auto write_dest = [&](size_t i, uint64_t value){
        std::string u = regU(i);
        unsigned r = reg_index_from_token(u);
        if (r == Registers::XZR_INDEX) return;              // write ignored
        if (r == Registers::XZR_INDEX + 1) { regs.writeSP(value); return; }
        if (r == 999) throw std::runtime_error("invalid dest register");
        if (is_w_reg_token(u)) regs.writeW(r, static_cast<uint32_t>(value));
        else                   regs.writeX(r, value);
    };

    auto read_imm64 = [&](size_t i)->uint64_t {
        if (!operand_is_imm(i)) throw std::runtime_error("immediate expected");
        return static_cast<uint64_t>(ops[i].imm);
    };

    // ---- execute
    if (up == "NOP") {
        // do nothing
    }
    else if (up == "MOV") {
        // MOV Rd, Rn | #imm
        if (ops.size() != 2) throw std::runtime_error("MOV expects 2 operands");
        uint64_t v = operand_is_imm(1) ? read_imm64(1) : read_src64(1);
        write_dest(0, v);
    }
    else if (up == "ADD" || up == "SUB" || up == "AND" || up == "EOR" || up == "MUL") {
        if (ops.size() != 3) throw std::runtime_error(up + " expects 3 operands");
        uint64_t a = read_src64(1);
        uint64_t b = operand_is_imm(2) ? read_imm64(2) : read_src64(2);

        uint64_t res = 0;
        if (up == "ADD")      res = a + b;
        else if (up == "SUB") res = a - b;
        else if (up == "AND") res = (a & b);
        else if (up == "EOR") res = (a ^ b);
        else                  res = (a * b); // MUL (low 64)
        write_dest(0, res);
        // (No flags unless you add *S variants; CMP handles flags)
    }
    else if (up == "CMP") {
        // CMP Rn, Rm | #imm    (sets N,Z,C,V like SUBS)
        if (ops.size() != 2 || !operand_is_reg(0))
            throw std::runtime_error("CMP expects Rn, (Rm|#imm)");
        std::string rnU = regU(0);
        uint64_t a = read_src64(0);
        uint64_t b = operand_is_imm(1) ? read_imm64(1) : read_src64(1);

        if (is_w_reg_token(rnU)) {
            uint32_t aa = static_cast<uint32_t>(a);
            uint32_t bb = static_cast<uint32_t>(b);
            uint32_t rr = static_cast<uint32_t>(aa - bb);
            set_flags_sub_32(regs.state(), aa, bb, rr);
        } else {
            uint64_t rr = a - b;
            set_flags_sub_64(regs.state(), a, b, rr);
        }
    }
    else if (up == "LDR" || up == "LDRB") {
        // LDR Rt, [base{,#off}]   /   LDRB Rt, [base{,#off}]
        if (ops.size() != 2 || !operand_is_reg(0) || !operand_is_mem(1))
            throw std::runtime_error(up + " expects Rt, [base{,#off}]");

        uint64_t ea = eff_addr_from_mem_token(ops[1], regs);

        if (up == "LDRB") {
            uint8_t byte = stack_read8(stack, ea);
            // write byte zero-extended into dest width
            std::string rtU = regU(0);
            if (is_w_reg_token(rtU)) write_dest(0, static_cast<uint32_t>(byte));
            else                      write_dest(0, static_cast<uint64_t>(byte));
        } else { // LDR
            std::string rtU = regU(0);
            if (is_w_reg_token(rtU)) {
                uint32_t w = stack_read32(stack, ea);         // 4 bytes, zero-extend
                write_dest(0, static_cast<uint64_t>(w));
            } else {
                uint64_t x = stack_read64(stack, ea);         // 8 bytes
                write_dest(0, x);
            }
        }
    }
    else if (up == "STR" || up == "STRB") {
        // STR Rt, [base{,#off}]   /  STRB Rt, [base{,#off}]
        if (ops.size() != 2 || !operand_is_reg(0) || !operand_is_mem(1))
            throw std::runtime_error(up + " expects Rt, [base{,#off}]");

        uint64_t ea = eff_addr_from_mem_token(ops[1], regs);
        std::string rtU = regU(0);

        if (up == "STRB") {
            uint64_t v = read_src64(0);
            stack_write8(stack, ea, static_cast<uint8_t>(v & 0xFF));
        } else { // STR
            if (is_w_reg_token(rtU)) {
                uint32_t w = static_cast<uint32_t>(read_src64(0));  // low 32
                stack_write32(stack, ea, w);                         // 4 bytes
            } else {
                uint64_t x = read_src64(0);
                stack_write64(stack, ea, x);                         // 8 bytes
            }
        }
    }

    else if (up == "B") {
        // B label
        if (ops.size() != 1 || ops[0].type != OperandType::Label)
            throw std::runtime_error("B expects a single label operand");
        std::string key = upper_copy(trim_copy(ops[0].raw));
        auto jt = prog.labels.find(key);
        if (jt == prog.labels.end()) throw std::runtime_error("undefined label: " + ops[0].raw);
        next_pc = jt->second;
    }
    else if (up == "B.GT" || up == "B.LE") {
        // Signed conditions with proper flags (set by CMP):
        //   GT: (Z==0) && (N==V)
        //   LE: (Z==1) || (N!=V)
        if (ops.size() != 1 || ops[0].type != OperandType::Label)
            throw std::runtime_error(up + " expects a single label operand");
        const auto& ps = regs.state();
        bool take = false;
        if (up == "B.GT") take = (!ps.Z && (ps.N == ps.V));
        else              take = ( ps.Z || (ps.N != ps.V));

        if (take) {
            std::string key = upper_copy(trim_copy(ops[0].raw));
            auto jt = prog.labels.find(key);
            if (jt == prog.labels.end()) throw std::runtime_error("undefined label: " + ops[0].raw);
            next_pc = jt->second;
        }
    }
    else if (up == "RET") {
        return false; // halt emulation
    }
    else {
        // Unimplemented mnem — treat as NOP or throw:
        // throw std::runtime_error("unimplemented instruction: " + up);
    }

    pc = next_pc;
    regs.writePC(pc);
    return (pc != end_addr);
}

} // namespace arm64