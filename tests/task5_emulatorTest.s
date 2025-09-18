// Task 5: all-in-one test program
// Exercises: NOP, MOV, ADD, SUB, AND, EOR, MUL, STR, LDR, STRB, LDRB,
//            CMP, B, B.GT, B.LE, RET
//
// Expected final state (high level):
//   X0 = 5
//   X1 = 3
//   X2 = 8                   ; 5 + 3
//   X3 = 0xFFFFFFFFFFFFFFFE  ; 8 - 10 = -2 (two's complement)
//   X4 = 8                   ; 8 & 0xF
//   X5 = 7                   ; 8 ^ 0xF
//   X6 = 15                  ; 5 * 3
//   X7 = 15                  ; reloaded from stack
//   X8 = 0xF                 ; LDRB into W8 → zero-extended
//   X9 = 0x1111              ; taken B.LE after equality (CMP X7, #15)
//   X10 = 0x22               ; branch GT taken for (8 > 5)
//   X11 = 0xFF
//   X12 = 0xFF               ; LDRB into X12 → zero-extended
// Stack (base 0x0):
//   [0x20..0x27] = 0F 00 00 00 00 00 00 00   (STR X6, [SP,#32])
//   [0x00]      = 0F                         (STRB X7, [SP,#0])
//   [0x01]      = FF                         (STRB X11,[SP,#1])

start:
  NOP

  // Basic arithmetic & moves
  MOV X0, #5
  MOV X1, #3
  ADD X2, X0, X1              // X2 = 8
  SUB X3, X2, #10             // X3 = -2 (wraps to 0xFFFFFFFFFFFFFFFE)
  AND X4, X2, #0xF            // X4 = 8
  EOR X5, X2, #0xF            // X5 = 7
  MUL X6, X0, X1              // X6 = 15

  // 64-bit stack store/load
  STR X6, [SP, #32]           // [0x20..0x27] = 0F 00 00 00 00 00 00 00
  LDR X7, [SP, #32]           // X7 = 15

  // Byte store/load + W-reg zero-extend
  STRB X7, [SP, #0]           // [0x00] = 0F
  LDRB W8, [SP, #0]           // W8 = 0x0F → X8 = 0x000...00F

  // Flag-setting compare + conditional branches
  CMP X7, #15                 // Z=1, N=0, C=1, V=0 (equal)
  B.GT bigger                 // not taken (equal)
  B.LE le_or_eq               // taken (equal qualifies)

bigger:
  MOV X9, #0xdeadbeef         // would set if greater
  B after_cmp

le_or_eq:
  MOV X9, #0x1111

after_cmp:
  // Unconditional branch test (skip this NOP)
  B next
  NOP

next:
  // Another flag/branch set: 8 > 5 → GT taken
  CMP X2, #5
  B.GT gt_path
  B.LE le_path

gt_path:
  MOV X10, #0x22
  B bytes

le_path:
  MOV X10, #0x33

bytes:
  // Another byte path: write FF, read back into X-reg via LDRB
  MOV X11, #0xFF
  STRB X11, [SP, #1]          // [0x01] = FF
  LDRB X12, [SP, #1]          // X12 = 0xFF (zero-extended)

  // All done
  RET
