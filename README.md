ARM64 Mini Emulator (Tasks 1–5)

A compact, didactic ARM64 (AArch64) emulator that parses a tiny assembly subset, assigns addresses, and executes instructions against a 256-byte stack. Designed as a step-by-step project:

Task 1 – Parse & pretty-print instructions.

Task 2 – Implement registers (X0–X30, XZR/WZR, SP, PC) + flags (N/Z[/C/V]).

Task 3 – Add a 256-byte stack and hex+ASCII dump.

Task 4 – Assign addresses (PC = 0x0, +4 per instruction) and honor labels/branches.

Task 5 – Execute instructions; add correct 32-bit (Wn) behavior.

Features

Instruction set:
ADD, SUB, AND, EOR, MUL, MOV, STR, STRB, LDR, LDRB, CMP, B, B.GT, B.LE, NOP, RET

32/64-bit correctness:

Reading Wn uses the low 32 bits.

Writing Wn writes the low 32 and zeroes the upper 32 (zero-extend).

LDR/STR follow destination/source width: W = 4 bytes, X = 8 bytes.

LDRB/STRB are 1 byte.

Minimal memory model: 256-byte stack, little-endian, bounds-checked.

Human-friendly printing for instructions, registers, and stack.

Labels + linear addresses (0x0, 0x4, 0x8, …) with proper branching.

Repository layout
include/
  executor.hpp     # program building + single-step executor (Task 4/5)
  parser.hpp       # decoding into {mnemonic, operands} (Task 1)
  registers.hpp    # X0..X30, XZR/WZR, SP, PC + flags (Task 2)
  stack.hpp        # 256-byte stack model (Task 3)

src/
  executor.cpp           # step() implementation; address/label builder
  executor_main.cpp      # driver main (parse → execute)
  parser.cpp             # parseLine() + operand parsing/formatting
  parser_main.cpp        # Task 1 pretty-printer (optional demo)
  registers.cpp          # (mostly header-only; keeps IDEs happy)
  registers_main.cpp     # Task 2 demo (print registers)
  stack.cpp              # (thin TU for the stack header)
  stack_main.cpp         # Task 3 demo (dump stack)

tests/
  task5/
    all_in_one.s         # single-file test exercises all instructions
  test_code_to_emulate/
    simple/test1/test1.txt  # example objdump-style input

Build
Requirements

CMake 3.15+

A C++17 compiler

Windows: Visual Studio Build Tools (MSBuild) or Ninja/MinGW

macOS/Linux: clang++/g++

Configure & build (Windows – MSBuild)
cmake -S . -B build
cmake --build build --config Debug
# executables land in build/Debug/

Configure & build (Ninja, flat output)
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
# executables land in build/


Using MSBuild? Executables are in build/Debug/ or build/Release/.
Using Ninja? Executables are directly in build/.

Running
1) Pretty-printer (Task 1 demo)
# MSBuild
./build/Debug/parser.exe ./tests/test_code_to_emulate/simple/test1/test1.txt

# Ninja
./build/parser ./tests/test_code_to_emulate/simple/test1/test1.txt

2) Registers (Task 2 demo)
./build/Debug/registers.exe

3) Stack (Task 3 demo)
./build/Debug/stack.exe         # zeroed stack
./build/Debug/stack.exe --random  # fill with deterministic random bytes

4) Emulator / Executor (Tasks 4–5)

The main driver is executor.exe (or emulator.exe if you named it that). It supports a couple of flags:

./build/Debug/executor.exe <input.s|txt> [--dump-regs] [--dump-stack] [--random-stack]


--dump-regs – print register file after execution.

--dump-stack – print 256-byte stack after execution.

--random-stack – fill the stack with random bytes before start.

Important: The emulator initializes SP to the top of the stack (base+size), since ARM64 stacks grow down.

Input format & parsing rules

Accepts both plain assembly and objdump-style lines:

  18: 910043ff  add sp, sp, #0x10


The parser automatically strips:

Optional address prefix: HEXADDR:

One leading 8-hex-digit opcode token: d10043ff

Inline comments (// or ;) outside memory brackets [...].

Labels end with : and may appear before an instruction, e.g.:

start:
  MOV X0, #5
loop: ADD X1, X1, #1
      B loop


Operands supported:

Registers: Xn, Wn, SP, XZR/WZR

Immediates: #10, #0x10

Memory: [SP], [SP, #imm], [Xn, #imm]

(By default we do not parse shifts like , LSL #1 unless you extended the parser.)

Instruction semantics (summary)

ALU: ADD, SUB, AND, EOR, MUL, MOV
Rd = Rn (op) (Rm|#imm). Width follows Rd (W=32 bit with zero-extend, X=64 bit).
Flags are not updated by these (only CMP sets flags).

Compare: CMP Rn, (Rm|#imm)
Sets N/Z/C/V as if Rn - op2 in the width of Rn.

Branches:

B label – unconditional.

B.GT – signed greater-than: (!Z && N==V)

B.LE – signed <=: (Z || N!=V)

Memory:

STR Rt, [base{,#off}] – 64-bit if Rt is Xn; 32-bit if Rt is Wn.

LDR Rt, [base{,#off}] – likewise; W zero-extends to 64 in dest.

STRB/LDRB – byte store/load (always 1 byte).

Base can be SP or Xn. Offsets support #imm or 0x...

Misc:

NOP – no operation.

RET – stop emulation.

Memory model

Single contiguous 256-byte stack at base address 0x0 (little-endian).

Emulation starts with SP = base + size (top of stack); the stack grows down.

Bounds-checked: all loads/stores throw if outside [base, base+size).

Quick end-to-end test

Use the single-file test that exercises every instruction:

tests/task5/all_in_one.s


Run:

./build/Debug/executor.exe ./tests/task5/all_in_one.s --dump-regs --dump-stack


You should see, among other values:

X2 = 0x0000000000000008

X3 = 0xfffffffffffffffe

X6 = 0x000000000000000f

X8 = 0x000000000000000f

X9 = 0x0000000000001111

X10 = 0x0000000000000022

X12 = 0x00000000000000ff

Stack line 00000020 contains: 0f 00 00 00 00 00 00 00

CTest (optional automation)

Add to the bottom of CMakeLists.txt:

include(CTest)

add_test(NAME t5_all
  COMMAND $<TARGET_FILE:executor> ${CMAKE_SOURCE_DIR}/tests/task5/all_in_one.s --dump-regs --dump-stack
)
set_tests_properties(t5_all PROPERTIES
  PASS_REGULAR_EXPRESSION "X2: 0x0000000000000008"
)


Then:

ctest --test-dir build -C Debug --output-on-failure

Troubleshooting

Executable not found (./build/parser):
MSBuild is multi-config; the exe is in build/Debug/ or build/Release/. Use --config and run from there. Or switch to Ninja for flat output.

stack write/read out of range:
Make sure SP starts at the top of the stack:
regs.writeSP(stack.base() + stack.size());

ADD third operand must be register or immediate:
We don’t accept memory or shifts in ADD. Use LDR/STR for memory and precompute shifts, unless you extended parser/executor to support , LSL #imm.

Mnemonic printed as SP, or opcode as instruction:
Ensure parseLine only strips the first token if it’s exactly 8 hex digits (the opcode). Do not drop arbitrary hex-looking tokens; ADD would be eaten.

MSVC C4244 in toupper:
Use:

std::transform(s.begin(), s.end(), s.begin(),
               [](unsigned char c){ return static_cast<char>(std::toupper(c)); });


Unresolved printDecoded / print_decoded:
Keep a tiny shim in parser.hpp:

inline void printDecoded(std::size_t n, const DecodedInstruction& d){ print_decoded(n,d); }

Authors: Kyle Mather and Braeden Allen — headers carry author tags. This project is for learning/educational purposes and intentionally omits many ARM64 features (vector/FP, complex addressing modes, pipelines, exceptions, etc.)
