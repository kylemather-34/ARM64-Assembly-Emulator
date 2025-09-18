#include <iostream>
#include <iomanip>
#include <string>

#include "parser.hpp"
#include "executor.hpp"
#include "registers.hpp"

using namespace arm64;

static std::string hex64(uint64_t v) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::nouppercase
       << std::setfill('0') << std::setw(16) << v;
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <input.asm>\n";
        return 1;
    }

    try {
        Parser parser;
        auto prog = build_program_from_file(argv[1], parser);

        if (prog.code.empty()) {
            std::cerr << "No instructions parsed.\n";
            return 0;
        }

        Registers regs;
        uint64_t pc = 0;
        regs.writePC(pc);

        // Safety to avoid accidental infinite loops
        const std::size_t kMaxSteps = 100000;
        std::size_t steps = 0;

        while (true) {
            if (++steps > kMaxSteps) {
                std::cerr << "Aborting: exceeded max step count (" << kMaxSteps << ")\n";
                return 2;
            }

            // If we're at end, done.
            if (pc == prog.code.size() * 4ull) {
                break;
            }

            // Fetch current instruction by PC
            auto it = prog.addr2idx.find(pc);
            if (it == prog.addr2idx.end()) {
                std::cerr << "PC points to unknown address: " << hex64(pc) << "\n";
                return 3;
            }
            const AsmInst& ai = prog.code[it->second];

            // Show PC for this instruction, then pretty-print like Task 1
            std::cout << "PC: " << hex64(pc) << "\n";
            printDecoded(ai.instr_index, ai.inst);

            // Step / update PC
            bool cont = step(prog, regs, pc);
            if (!cont) break;
        }

        std::cout << "Program finished. Final PC = " << hex64(pc) << "\n";
        return 0;

    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 4;
    }
}