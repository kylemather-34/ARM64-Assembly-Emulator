// src/executor_main.cpp
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include "parser.hpp"
#include "executor.hpp"   // build_program_from_file(...) and step(...)
#include "registers.hpp"
#include "stack.hpp"

using namespace arm64;

static std::string hex64(uint64_t v) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::nouppercase
       << std::setfill('0') << std::setw(16) << v;
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr
            << "usage: " << argv[0] << " <input.asm> [--dump-regs] [--dump-stack] [--random-stack]\n";
        return 1;
    }

    const std::string path = argv[1];
    bool dump_regs = false, dump_stack = false, random_stack = false;
    for (int i = 2; i < argc; ++i) {
        std::string f = argv[i];
        if (f == "--dump-regs")      dump_regs = true;
        else if (f == "--dump-stack") dump_stack = true;
        else if (f == "--random-stack") random_stack = true;
        else { std::cerr << "unknown flag: " << f << "\n"; return 1; }
    }

    try {
        Parser parser;
        // Task 4: parse file into linear program with addresses/labels
        AsmProgram prog = build_program_from_file(path, parser);
        if (prog.code.empty()) {
            std::cerr << "No instructions parsed from: " << path << "\n";
            return 0;
        }

        // Task 2/3: set up registers and 256-byte stack, SP = stack base
        Registers regs;
        Stack stack(/*base=*/0x0);
        if (random_stack) stack.fillRandom();
        regs.writeSP(stack.base());

        // Start PC at 0x0 (advances by 4; branches handled in step)
        uint64_t pc = 0;
        regs.writePC(pc);

        // Safety guard for accidental infinite loops
        const std::size_t kMaxSteps = 100000;
        std::size_t steps = 0;

        while (true) {
            if (++steps > kMaxSteps) {
                std::cerr << "Aborting: exceeded max step count (" << kMaxSteps << ")\n";
                break;
            }
            if (pc == prog.code.size() * 4ull) break; // fell off end

            auto it = prog.addr2idx.find(pc);
            if (it == prog.addr2idx.end()) {
                std::cerr << "PC points to unknown address: " << hex64(pc) << "\n";
                break;
            }
            const AsmInst& ai = prog.code[it->second];

            // Show PC and the formatted instruction (Task 1 pretty-printer)
            std::cout << "PC: " << hex64(pc) << "\n";
            printDecoded(ai.instr_index, ai.inst); // if you only have printDecoded, use that

            // Task 5: execute one instruction; returns false on RET or natural end
            if (!step(prog, regs, stack, pc)) break;
        }

        std::cout << "Program finished. Final PC = " << hex64(pc) << "\n\n";
        if (dump_regs)  regs.print(std::cout);
        if (dump_stack) stack.printDump(std::cout);
        return 0;

    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 2;
    }
}
