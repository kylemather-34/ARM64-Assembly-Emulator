start:
  // W writes zero-extend, arithmetic wraps to 32 bits
  MOV W0, #0xFFFFFFFF         // X0 = 0x00000000FFFFFFFF
  ADD W0, W0, #1              // X0 = 0x0000000000000000

  // 32-bit stores/loads
  MOV W1, #0xAABBCCDD
  STR W1, [SP, #0]            // write 4 bytes
  LDR W2, [SP, #0]            // read 4 bytes -> W2 = 0xAABBCCDD
  LDR X3, [SP, #0]            // (optional) undefined high 4 bytes in real HW; in our model,
                              // it'll read 8 bytes (first 4 are your STR W1; next 4 are whatever's in memory)

  // Byte paths still fine
  STRB W1, [SP, #8]           // DD
  LDRB X4, [SP, #8]           // X4 = 0xDD

  RET