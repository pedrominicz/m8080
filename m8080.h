/* Copyright (c) 2019 Pedro Minicz */
#ifndef M8080_H
#define M8080_H
// the user must define M8080_IMPLEMENTATION in exactly one file that includes
// this header before the include:
//
//      #define M8080_IMPLEMENTATION
//      #include "m8080.h"
//
// all other files should just include without the define

#include <stddef.h>
#include <stdint.h>

typedef struct m8080 {
  struct {
    uint8_t c; // carry
    uint8_t p; // parity bit
    uint8_t a; // auxiliary carry
    uint8_t z; // zero bit
    uint8_t s; // sign bit
  } f;
  uint8_t a; // accumulator
  // 6 8-bit registers that double as 3 16-bit registers
  union { struct { uint8_t c, b; }; uint16_t bc; };
  union { struct { uint8_t e, d; }; uint16_t de; };
  union { struct { uint8_t l, h; }; uint16_t hl; };
  uint16_t sp; // stack pointer
  uint16_t pc; // program counter
  uint8_t inte; // interrupt enable
  size_t cycles;
  void* userdata;
} m8080;

// restart instruction subroutine call addresses
enum {
  M8080_RST_0 = 0x0000, M8080_RST_1 = 0x0008,
  M8080_RST_2 = 0x0010, M8080_RST_3 = 0x0018,
  M8080_RST_4 = 0x0020, M8080_RST_5 = 0x0028,
  M8080_RST_6 = 0x0030, M8080_RST_7 = 0x0038,
};

// prints the instruction in buffer[pc] to stdout and returns it's size
size_t m8080_disassemble(const uint8_t* const buffer, const size_t pc);

size_t m8080_step(m8080* const c);
// when the original 8080 recognizes an interrupt request from an external
// device, the following actions occur:
//
//  (1) the instruction currently being executed is completed
//  (2) the interrupt enable bit is reset
//  (3) the interrupting device supplies, via hardware, one instruction which
//      the 8080 executes
//
// the instruction supplied by the interrupting device is usually an RST
// instruction since it is an efficient one-byte call to one of 8 eight-byte
// subroutines located in the first 64 bytes of memory
//
// this emulator supply interrupts as a "call if interrupt enable" to an
// arbitrary address A, the interrupt enable bit is reset as expected
size_t m8080_interrupt(m8080* const c, const uint16_t a);

// the user is expected to implement the following five functions

// read byte in memory address A
uint8_t m8080_rb(const m8080* const c, const uint16_t a);
// write byte B to memory address A
void m8080_wb(m8080* const c, const uint16_t a, const uint8_t b);

// the contents of input device A are read into the accumulator
void m8080_in(m8080* const c, const uint8_t a);
// the contents of the accumulator are sent to output device A
void m8080_out(m8080* const c, const uint8_t a);
// halt instruction
void m8080_hlt(m8080* const c);

#endif // M8080_H

#ifdef M8080_IMPLEMENTATION
#undef M8080_IMPLEMENTATION

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static const size_t m8080_cycles[] = {
  4, 10,7, 5, 5, 5, 7, 4, 4, 10,7, 5, 5, 5, 7, 4, // 00..0f
  4, 10,7, 5, 5, 5, 7, 4, 4, 10,7, 5, 5, 5, 7, 4, // 00..1f
  4, 10,16,5, 5, 5, 7, 4, 4, 10,16,5, 5, 5, 7, 4, // 20..2f
  4, 10,13,5, 10,10,10,4, 4, 10,13,5, 5, 5, 7, 4, // 30..3f
  5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5, // 40..4f
  5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5, // 50..5f
  5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5, // 60..6f
  7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5, 5, 5, 7, 5, // 70..7f
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // 80..8f
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // 90..9f
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // a0..af
  4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // b0..bf
  5, 10,10,10,11,11,7, 11,5, 10,10,10,11,17,7,11, // c0..cf
  5, 10,10,10,11,11,7, 11,5, 10,10,10,11,17,7,11, // d0..df
  5, 10,10,18,11,11,7, 11,5, 5, 10,5, 11,17,7,11, // e0..ef
  5, 10,10,4, 11,11,7, 11,5, 5, 10,4, 11,17,7,11, // f0..ff
};

// 1 for even parity and 0 for odd parity
static const uint8_t m8080_parity[] = {
  1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0, // 00..1f
  0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1, // 20..3f
  0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1, // 40..5f
  1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0, // 60..7f
  0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1, // 80..9f
  1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0, // a0..bf
  1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0, // c0..df
  0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1, // e0..ff
};

size_t m8080_disassemble(const uint8_t* const buffer, const size_t pc) {
  const uint8_t opcode = buffer[pc];
  printf("%04zx ", pc);

#define I1(s) printf("%02x       " s "\n", opcode); return 1;
#define I2(s) printf("%02x %02x    " s "\n", opcode, buffer[pc + 1], buffer[pc + 1]); return 2;
#define I3(s) printf("%02x %02x %02x " s "\n", opcode, buffer[pc + 1], buffer[pc + 2], buffer[pc + 2], buffer[pc + 1]); return 3;
  switch(opcode) {
  // set carry
  case 0x37: I1("STC");

  // complement carry
  case 0x3f: I1("CMC");

  // increment register or memory
  case 0x04: I1("INR    B");
  case 0x0c: I1("INR    C");
  case 0x14: I1("INR    D");
  case 0x1c: I1("INR    E");
  case 0x24: I1("INR    H");
  case 0x2c: I1("INR    L");
  case 0x34: I1("INR    M");
  case 0x3c: I1("INR    A");

  // decrement register or memory
  case 0x05: I1("DCR    B");
  case 0x0d: I1("DCR    C");
  case 0x15: I1("DCR    D");
  case 0x1d: I1("DCR    E");
  case 0x25: I1("DCR    H");
  case 0x2d: I1("DCR    L");
  case 0x35: I1("DCR    M");
  case 0x3d: I1("DCR    A");

  // complement accumulator
  case 0x2f: I1("CMA");

  // decimal adjust accumulator
  case 0x27: I1("DAA");

  // no operation instructions
  case 0x00: I1("NOP");
  case 0x08: I1("NOP"); // undocumented
  case 0x10: I1("NOP"); // undocumented
  case 0x18: I1("NOP"); // undocumented
  case 0x20: I1("NOP"); // undocumented
  case 0x28: I1("NOP"); // undocumented
  case 0x30: I1("NOP"); // undocumented
  case 0x38: I1("NOP"); // undocumented

  // move
  case 0x40: I1("MOV    B,B");
  case 0x41: I1("MOV    B,C");
  case 0x42: I1("MOV    B,D");
  case 0x43: I1("MOV    B,E");
  case 0x44: I1("MOV    B,H");
  case 0x45: I1("MOV    B,L");
  case 0x46: I1("MOV    B,M");
  case 0x47: I1("MOV    B,A");
  case 0x48: I1("MOV    C,B");
  case 0x49: I1("MOV    C,C");
  case 0x4a: I1("MOV    C,D");
  case 0x4b: I1("MOV    C,E");
  case 0x4c: I1("MOV    C,H");
  case 0x4d: I1("MOV    C,L");
  case 0x4e: I1("MOV    C,M");
  case 0x4f: I1("MOV    C,A");
  case 0x50: I1("MOV    D,B");
  case 0x51: I1("MOV    D,C");
  case 0x52: I1("MOV    D,D");
  case 0x53: I1("MOV    D,E");
  case 0x54: I1("MOV    D,H");
  case 0x55: I1("MOV    D,L");
  case 0x56: I1("MOV    D,M");
  case 0x57: I1("MOV    D,A");
  case 0x58: I1("MOV    E,B");
  case 0x59: I1("MOV    E,C");
  case 0x5a: I1("MOV    E,D");
  case 0x5b: I1("MOV    E,E");
  case 0x5c: I1("MOV    E,H");
  case 0x5d: I1("MOV    E,L");
  case 0x5e: I1("MOV    E,M");
  case 0x5f: I1("MOV    E,A");
  case 0x60: I1("MOV    H,B");
  case 0x61: I1("MOV    H,C");
  case 0x62: I1("MOV    H,D");
  case 0x63: I1("MOV    H,E");
  case 0x64: I1("MOV    H,H");
  case 0x65: I1("MOV    H,L");
  case 0x66: I1("MOV    H,M");
  case 0x67: I1("MOV    H,A");
  case 0x68: I1("MOV    L,B");
  case 0x69: I1("MOV    L,C");
  case 0x6a: I1("MOV    L,D");
  case 0x6b: I1("MOV    L,E");
  case 0x6c: I1("MOV    L,H");
  case 0x6d: I1("MOV    L,L");
  case 0x6e: I1("MOV    L,M");
  case 0x6f: I1("MOV    L,A");
  case 0x70: I1("MOV    M,B");
  case 0x71: I1("MOV    M,C");
  case 0x72: I1("MOV    M,D");
  case 0x73: I1("MOV    M,E");
  case 0x74: I1("MOV    M,H");
  case 0x75: I1("MOV    M,L");
  case 0x77: I1("MOV    M,A");
  case 0x78: I1("MOV    A,B");
  case 0x79: I1("MOV    A,C");
  case 0x7a: I1("MOV    A,D");
  case 0x7b: I1("MOV    A,E");
  case 0x7c: I1("MOV    A,H");
  case 0x7d: I1("MOV    A,L");
  case 0x7e: I1("MOV    A,M");
  case 0x7f: I1("MOV    A,A");

  // store accumulator
  case 0x02: I1("STAX   B");
  case 0x12: I1("STAX   D");

  // load accumulator
  case 0x0a: I1("LDAX   B");
  case 0x1a: I1("LDAX   D");

  // add register or memory to accumulator
  case 0x80: I1("ADD    B");
  case 0x81: I1("ADD    C");
  case 0x82: I1("ADD    D");
  case 0x83: I1("ADD    E");
  case 0x84: I1("ADD    H");
  case 0x85: I1("ADD    L");
  case 0x86: I1("ADD    M");
  case 0x87: I1("ADD    A");

  // add register or memory to accumulator with carry
  case 0x88: I1("ADC    B");
  case 0x89: I1("ADC    C");
  case 0x8a: I1("ADC    D");
  case 0x8b: I1("ADC    E");
  case 0x8c: I1("ADC    H");
  case 0x8d: I1("ADC    L");
  case 0x8e: I1("ADC    M");
  case 0x8f: I1("ADC    A");

  // subtract register or memory from accumulator
  case 0x90: I1("SUB    B");
  case 0x91: I1("SUB    C");
  case 0x92: I1("SUB    D");
  case 0x93: I1("SUB    E");
  case 0x94: I1("SUB    H");
  case 0x95: I1("SUB    L");
  case 0x96: I1("SUB    M");
  case 0x97: I1("SUB    A");

  // subtract register or memory from accumulator with borrow
  case 0x98: I1("SBB    B");
  case 0x99: I1("SBB    C");
  case 0x9a: I1("SBB    D");
  case 0x9b: I1("SBB    E");
  case 0x9c: I1("SBB    H");
  case 0x9d: I1("SBB    L");
  case 0x9e: I1("SBB    M");
  case 0x9f: I1("SBB    A");

  // logical AND register or memory with accumulator
  case 0xa0: I1("ANA    B");
  case 0xa1: I1("ANA    C");
  case 0xa2: I1("ANA    D");
  case 0xa3: I1("ANA    E");
  case 0xa4: I1("ANA    H");
  case 0xa5: I1("ANA    L");
  case 0xa6: I1("ANA    M");
  case 0xa7: I1("ANA    A");

  // logical XOR register or memory with accumulator
  case 0xa8: I1("XRA    B");
  case 0xa9: I1("XRA    C");
  case 0xaa: I1("XRA    D");
  case 0xab: I1("XRA    E");
  case 0xac: I1("XRA    H");
  case 0xad: I1("XRA    L");
  case 0xae: I1("XRA    M");
  case 0xaf: I1("XRA    A");

  // logical OR register or memory with accumulator
  case 0xb0: I1("ORA    B");
  case 0xb1: I1("ORA    C");
  case 0xb2: I1("ORA    D");
  case 0xb3: I1("ORA    E");
  case 0xb4: I1("ORA    H");
  case 0xb5: I1("ORA    L");
  case 0xb6: I1("ORA    M");
  case 0xb7: I1("ORA    A");

  // compare register or memory with accumulator
  case 0xb8: I1("CMP    B");
  case 0xb9: I1("CMP    C");
  case 0xba: I1("CMP    D");
  case 0xbb: I1("CMP    E");
  case 0xbc: I1("CMP    H");
  case 0xbd: I1("CMP    L");
  case 0xbe: I1("CMP    M");
  case 0xbf: I1("CMP    A");

  // rotate accumulator instructions
  case 0x07: I1("RLC");
  case 0x0f: I1("RRC");
  case 0x17: I1("RAL");
  case 0x1f: I1("RAR");

  // push data onto stack
  case 0xc5: I1("PUSH   B");
  case 0xd5: I1("PUSH   D");
  case 0xe5: I1("PUSH   H");
  case 0xf5: I1("PUSH   PSW");

  // pop data off stack
  case 0xc1: I1("POP    B");
  case 0xd1: I1("POP    D");
  case 0xe1: I1("POP    H");
  case 0xf1: I1("POP    PSW");

  // double add
  case 0x09: I1("DAD    B");
  case 0x19: I1("DAD    D");
  case 0x29: I1("DAD    H");
  case 0x39: I1("DAD    SP");

  // increment register pair
  case 0x03: I1("INX    B");
  case 0x13: I1("INX    D");
  case 0x23: I1("INX    H");
  case 0x33: I1("INX    SP");

  // decrement register pair
  case 0x0b: I1("DCX    B");
  case 0x1b: I1("DCX    D");
  case 0x2b: I1("DCX    H");
  case 0x3b: I1("DCX    SP");

  // exchange registers
  case 0xeb: I1("XCHG");
  case 0xe3: I1("XTHL");
  case 0xf9: I1("SPHL");

  // move immediate word
  case 0x01: I3("LXI    B,%02x%02x");
  case 0x11: I3("LXI    D,%02x%02x");
  case 0x21: I3("LXI    H,%02x%02x");
  case 0x31: I3("LXI    SP,%02x%02x");

  // move immediate byte
  case 0x06: I2("MVI    B,%02x");
  case 0x0e: I2("MVI    C,%02x");
  case 0x16: I2("MVI    D,%02x");
  case 0x1e: I2("MVI    E,%02x");
  case 0x26: I2("MVI    H,%02x");
  case 0x2e: I2("MVI    L,%02x");
  case 0x36: I2("MVI    M,%02x");
  case 0x3e: I2("MVI    A,%02x");

  // immediate instructions
  case 0xc6: I2("ADI    %02x");
  case 0xce: I2("ACI    %02x");
  case 0xd6: I2("SUI    %02x");
  case 0xde: I2("SBI    %02x");
  case 0xe6: I2("ANI    %02x");
  case 0xee: I2("XRI    %02x");
  case 0xf6: I2("ORI    %02x");
  case 0xfe: I2("CPI    %02x");

  // store/load accumulator direct
  case 0x32: I3("STA    %02x%02x");
  case 0x3a: I3("LDA    %02x%02x");

  // store/load HL direct
  case 0x22: I3("SHLD   %02x%02x");
  case 0x2a: I3("LHLD   %02x%02x");

  // load program counter
  case 0xe9: I1("PCHL");

  // jump instructions
  case 0xc3: I3("JMP    %02x%02x");
  case 0xcb: I3("JMP    %02x%02x"); // undocumented
  case 0xda: I3("JC     %02x%02x");
  case 0xd2: I3("JNC    %02x%02x");
  case 0xca: I3("JZ     %02x%02x");
  case 0xc2: I3("JNZ    %02x%02x");
  case 0xfa: I3("JM     %02x%02x");
  case 0xf2: I3("JP     %02x%02x");
  case 0xea: I3("JPE    %02x%02x");
  case 0xe2: I3("JPO    %02x%02x");

  // call subroutine instructions
  case 0xcd: I3("CALL   %02x%02x");
  case 0xdd: I3("CALL   %02x%02x"); // undocumented
  case 0xed: I3("CALL   %02x%02x"); // undocumented
  case 0xfd: I3("CALL   %02x%02x"); // undocumented
  case 0xdc: I3("CC     %02x%02x");
  case 0xd4: I3("CNC    %02x%02x");
  case 0xcc: I3("CZ     %02x%02x");
  case 0xc4: I3("CNZ    %02x%02x");
  case 0xfc: I3("CM     %02x%02x");
  case 0xf4: I3("CP     %02x%02x");
  case 0xec: I3("CPE    %02x%02x");
  case 0xe4: I3("CPO    %02x%02x");

  // return from subroutine instructions
  case 0xc9: I1("RET");
  case 0xd9: I1("RET"); // undocumented
  case 0xd8: I1("RC");
  case 0xd0: I1("RNC");
  case 0xc8: I1("RZ");
  case 0xc0: I1("RNZ");
  case 0xf8: I1("RM");
  case 0xf0: I1("RP");
  case 0xe8: I1("RPE");
  case 0xe0: I1("RPO");

  // restart instructions
  case 0xc7: I1("RST    0");
  case 0xcf: I1("RST    1");
  case 0xd7: I1("RST    2");
  case 0xdf: I1("RST    3");
  case 0xe7: I1("RST    4");
  case 0xef: I1("RST    5");
  case 0xf7: I1("RST    6");
  case 0xff: I1("RST    7");

  // interrupt flip-flop instructions
  case 0xfb: I1("EI");
  case 0xf3: I1("DI");

  // input/output instructions
  case 0xdb: I2("IN     %02x");
  case 0xd3: I2("OUT    %02x");

  // halt instruction
  case 0x76: I1("HLT");
  }
#undef I1
#undef I2
#undef I3
}

// read word
static inline uint16_t m8080_rw(const m8080* const c, const uint16_t a) {
  return m8080_rb(c, a + 1) << 8 | m8080_rb(c, a);
}

// write word
static inline uint16_t m8080_ww(m8080* const c, const uint16_t a, const uint16_t w) {
  m8080_wb(c, a + 0, w);
  m8080_wb(c, a + 1, w >> 8);
  return m8080_rw(c, a);
}

static inline uint8_t m8080_next_byte(m8080* const c) {
  return m8080_rb(c, c->pc++);
}

static inline uint16_t m8080_next_word(m8080* const c) {
  const uint16_t ret = m8080_rw(c, c->pc);
  c->pc += 2;
  return ret;
}

static inline void m8080_set_pzs(m8080* const c, const uint8_t a) {
  c->f.p = m8080_parity[a];
  c->f.z = a == 0;
  c->f.s = a >> 7; // sign bit
}

// the number in the accumulator is adjusted to form two four-bit binary-coded
// decimal digits by the following process:
//
//  (1) if the least significant four bits of the accumulator represents a
//      number greater than 9, or the auxiliary carry is set, the accumulator
//      is incremented by six
//  (2) if the most significant four bits of the accumulator now represents a
//      number greater than 9, or the carry is set, the most significant four
//      bits of the accumulator are incremented by six
//
// if a carry out of the least significant four bits occurs during step 1, the
// auxiliary carry is set; otherwise, it is reset
//
// if a carry occurs during either step, the carry is set; otherwise, it is
// unaffected
static inline void m8080_daa(m8080* const c) {
  if(c->f.a || (c->a & 0x0f) > 0x09) {
    c->f.c |= (c->a + 0x06) >> 8; // carry can be set but not reset
    c->f.a = ((c->a & 0x0f) + 0x06) >> 4;
    c->a += 0x06;
  }
  if(c->f.c || (c->a & 0xf0) > 0x90) {
    c->f.c |= (c->a + 0x60) >> 8; // carry can be set but not reset
    c->a += 0x60;
  }
  m8080_set_pzs(c, c->a);
}

static inline void m8080_add(m8080* const c, const uint8_t a) {
  c->f.c = (c->a + a) >> 8;
  c->f.a = ((c->a & 0x0f) + (a & 0x0f)) >> 4;
  c->a += a;
  m8080_set_pzs(c, c->a);
}

static inline void m8080_adc(m8080* const c, const uint8_t a) {
  uint8_t tmp = c->f.c;
  c->f.c = (c->a + a + tmp) >> 8;
  c->f.a = ((c->a & 0x0f) + (a & 0x0f) + tmp) >> 4;
  c->a += a + tmp;
  m8080_set_pzs(c, c->a);
}

static inline void m8080_sub(m8080* const c, const uint8_t a) {
  c->f.c = (c->a - a) >> 8 & 0x01;
  c->f.a = ~((c->a & 0x0f) - (a & 0x0f)) >> 4 & 0x01;
  c->a -= a;
  m8080_set_pzs(c, c->a);
}

static inline void m8080_sbb(m8080* const c, const uint8_t a) {
  uint8_t tmp = c->f.c;
  c->f.c = (c->a - a - tmp) >> 8 & 0x01;
  c->f.a = ~((c->a & 0x0f) - (a & 0x0f) - tmp) >> 4 & 0x01;
  c->a -= a + tmp;
  m8080_set_pzs(c, c->a);
}

static inline void m8080_ana(m8080* const c, const uint8_t a) {
  c->f.c = 0;
  c->f.a = (c->a | a) >> 3 & 0x01;
  c->a &= a;
  m8080_set_pzs(c, c->a);
}

static inline void m8080_xra(m8080* const c, const uint8_t a) {
  c->a ^= a;
  m8080_set_pzs(c, c->a);
  c->f.c = 0;
  c->f.a = 0;
}

static inline void m8080_ora(m8080* const c, const uint8_t a) {
  c->a |= a;
  m8080_set_pzs(c, c->a);
  c->f.c = 0;
  c->f.a = 0;
}

static inline void m8080_cmp(m8080* const c, const uint8_t a) {
  uint8_t tmp = c->a;
  m8080_sub(c, a);
  c->a = tmp;
}

// rotate accumulator left
static inline void m8080_rlc(m8080* const c) {
  c->f.c = c->a >> 7;
  c->a = c->a << 1 | c->f.c;
}

// rotate accumulator right
static inline void m8080_rrc(m8080* const c) {
  c->f.c = c->a & 0x01;
  c->a = c->a >> 1 | c->f.c << 7;
}

// rotate accumulator left through carry
static inline void m8080_ral(m8080* const c) {
  const uint8_t tmp = c->a >> 7;
  c->a = c->a << 1 | c->f.c;
  c->f.c = tmp;
}

// rotate accumulator right through carry
static inline void m8080_rar(m8080* const c) {
  const uint8_t tmp = c->a & 0x01;
  c->a = c->a >> 1 | c->f.c << 7;
  c->f.c = tmp;
}

static inline void m8080_push(m8080* const c, const uint16_t a) {
  c->sp -= 2;
  m8080_ww(c, c->sp, a);
}

// the contents of PSW are saved in two bytes of memory indicated by the stack
// pointer, the first byte holds the contents of the accumulator and the second
// byte holds the settings of the five condition bits, the format of this byte
// is:
//
//      +---+---+---+---+---+---+---+---+
//      |f.s|f.z| 0 |f.a| 0 |f.p| 1 |f.c|
//      +---+---+---+---+---+---+---+---+
//
// note that bit 1 is always 1 and bits 3 and 5 always 0
static inline void m8080_push_psw(m8080* const c) {
  uint16_t tmp = c->a << 8 | 0x02; // bit 1 is always 1
  tmp |= c->f.c << 0;
  tmp |= c->f.p << 2;
  tmp |= c->f.a << 4;
  tmp |= c->f.z << 6;
  tmp |= c->f.s << 7;
  m8080_push(c, tmp);
}

static inline uint16_t m8080_pop(m8080* const c) {
  const uint16_t ret = m8080_rw(c, c->sp);
  c->sp += 2;
  return ret;
}

static inline void m8080_pop_psw(m8080* const c) {
  const uint16_t tmp = m8080_pop(c);
  c->a = tmp >> 8;
  c->f.c = tmp >> 0 & 0x01;
  c->f.p = tmp >> 2 & 0x01;
  c->f.a = tmp >> 4 & 0x01;
  c->f.z = tmp >> 6 & 0x01;
  c->f.s = tmp >> 7 & 0x01;
}

static inline void m8080_xchg(m8080* const c) {
  const uint16_t tmp = c->hl;
  c->hl = c->de;
  c->de = tmp;
}

static inline void m8080_xthl(m8080* const c) {
  const uint16_t tmp = c->hl;
  c->hl = m8080_rw(c, c->sp);
  m8080_ww(c, c->sp, tmp);
}

static inline void m8080_cond_jmp(m8080* const c, const uint8_t condition) {
  const uint16_t a = m8080_next_word(c);
  if(condition) c->pc = a;
}

static inline void m8080_call(m8080* const c, const uint16_t a) {
  m8080_push(c, c->pc);
  c->pc = a;
}

static inline void m8080_cond_call(m8080* const c, const uint8_t condition) {
  const uint16_t a = m8080_next_word(c);
  if(condition) {
    m8080_call(c, a);
    c->cycles += 6;
  }
}

static inline void m8080_cond_ret(m8080* const c, const uint8_t condition) {
  if(condition) {
    c->pc = m8080_pop(c);
    c->cycles += 6;
  }
}

size_t m8080_step(m8080* const c) {
  const uint8_t opcode = m8080_next_byte(c);
  const size_t previous_cycle = c->cycles;
  c->cycles += m8080_cycles[opcode];

  switch(opcode) {
  // set carry
  case 0x37: c->f.c = 1; break; // STC

  // complement carry
  case 0x3f: c->f.c = !c->f.c; break; // CMC

  // increment register or memory
  case 0x04: ++c->b; c->f.a = (c->b & 0x0f) == 0; m8080_set_pzs(c, c->b); break; // INR B
  case 0x0c: ++c->c; c->f.a = (c->c & 0x0f) == 0; m8080_set_pzs(c, c->c); break; // INR C
  case 0x14: ++c->d; c->f.a = (c->d & 0x0f) == 0; m8080_set_pzs(c, c->d); break; // INR D
  case 0x1c: ++c->e; c->f.a = (c->e & 0x0f) == 0; m8080_set_pzs(c, c->e); break; // INR E
  case 0x24: ++c->h; c->f.a = (c->h & 0x0f) == 0; m8080_set_pzs(c, c->h); break; // INR H
  case 0x2c: ++c->l; c->f.a = (c->l & 0x0f) == 0; m8080_set_pzs(c, c->l); break; // INR L
  case 0x34: { // INR M
    const uint8_t res = m8080_rb(c, c->hl) + 1;
    m8080_wb(c, c->hl, res);
    c->f.a = (res & 0x0f) == 0;
    m8080_set_pzs(c, res);
  } break;
  case 0x3c: ++c->a; c->f.a = (c->a & 0x0f) == 0; m8080_set_pzs(c, c->a); break; // INR A

  // decrement register or memory
  case 0x05: --c->b; c->f.a = (c->b & 0x0f) != 0x0f; m8080_set_pzs(c, c->b); break; // DCR B
  case 0x0d: --c->c; c->f.a = (c->c & 0x0f) != 0x0f; m8080_set_pzs(c, c->c); break; // DCR C
  case 0x15: --c->d; c->f.a = (c->d & 0x0f) != 0x0f; m8080_set_pzs(c, c->d); break; // DCR D
  case 0x1d: --c->e; c->f.a = (c->e & 0x0f) != 0x0f; m8080_set_pzs(c, c->e); break; // DCR E
  case 0x25: --c->h; c->f.a = (c->h & 0x0f) != 0x0f; m8080_set_pzs(c, c->h); break; // DCR H
  case 0x2d: --c->l; c->f.a = (c->l & 0x0f) != 0x0f; m8080_set_pzs(c, c->l); break; // DCR L
  case 0x35: { // DCR M
    const uint8_t res = m8080_rb(c, c->hl) - 1;
    m8080_wb(c, c->hl, res);
    c->f.a = (res & 0x0f) != 0x0f;
    m8080_set_pzs(c, res);
  } break;
  case 0x3d: --c->a; c->f.a = (c->a & 0x0f) != 0x0f; m8080_set_pzs(c, c->a); break; // DCR A

  // complement accumulator
  case 0x2f: c->a = ~c->a; break; // CMA

  // decimal adjust accumulator
  case 0x27: m8080_daa(c); break; // DAA

  // no operation instructions
  case 0x00: break; // NOP
  case 0x08: break; // NOP
  case 0x10: break; // NOP
  case 0x18: break; // NOP
  case 0x20: break; // NOP
  case 0x28: break; // NOP
  case 0x30: break; // NOP
  case 0x38: break; // NOP

  // move
  case 0x40: c->b = c->b; break; // MOV B,B
  case 0x41: c->b = c->c; break; // MOV B,C
  case 0x42: c->b = c->d; break; // MOV B,D
  case 0x43: c->b = c->e; break; // MOV B,E
  case 0x44: c->b = c->h; break; // MOV B,H
  case 0x45: c->b = c->l; break; // MOV B,L
  case 0x46: c->b = m8080_rb(c, c->hl); break; // MOV B,M
  case 0x47: c->b = c->a; break; // MOV B,A
  case 0x48: c->c = c->b; break; // MOV C,B
  case 0x49: c->c = c->c; break; // MOV C,C
  case 0x4a: c->c = c->d; break; // MOV C,D
  case 0x4b: c->c = c->e; break; // MOV C,E
  case 0x4c: c->c = c->h; break; // MOV C,H
  case 0x4d: c->c = c->l; break; // MOV C,L
  case 0x4e: c->c = m8080_rb(c, c->hl); break; // MOV C,M
  case 0x4f: c->c = c->a; break; // MOV C,A
  case 0x50: c->d = c->b; break; // MOV D,B
  case 0x51: c->d = c->c; break; // MOV D,C
  case 0x52: c->d = c->d; break; // MOV D,D
  case 0x53: c->d = c->e; break; // MOV D,E
  case 0x54: c->d = c->h; break; // MOV D,H
  case 0x55: c->d = c->l; break; // MOV D,L
  case 0x56: c->d = m8080_rb(c, c->hl); break; // MOV D,M
  case 0x57: c->d = c->a; break; // MOV D,A
  case 0x58: c->e = c->b; break; // MOV E,B
  case 0x59: c->e = c->c; break; // MOV E,C
  case 0x5a: c->e = c->d; break; // MOV E,D
  case 0x5b: c->e = c->e; break; // MOV E,E
  case 0x5c: c->e = c->h; break; // MOV E,H
  case 0x5d: c->e = c->l; break; // MOV E,L
  case 0x5e: c->e = m8080_rb(c, c->hl); break; // MOV E,M
  case 0x5f: c->e = c->a; break; // MOV E,A
  case 0x60: c->h = c->b; break; // MOV H,B
  case 0x61: c->h = c->c; break; // MOV H,C
  case 0x62: c->h = c->d; break; // MOV H,D
  case 0x63: c->h = c->e; break; // MOV H,E
  case 0x64: c->h = c->h; break; // MOV H,H
  case 0x65: c->h = c->l; break; // MOV H,L
  case 0x66: c->h = m8080_rb(c, c->hl); break; // MOV H,M
  case 0x67: c->h = c->a; break; // MOV H,A
  case 0x68: c->l = c->b; break; // MOV L,B
  case 0x69: c->l = c->c; break; // MOV L,C
  case 0x6a: c->l = c->d; break; // MOV L,D
  case 0x6b: c->l = c->e; break; // MOV L,E
  case 0x6c: c->l = c->h; break; // MOV L,H
  case 0x6d: c->l = c->l; break; // MOV L,L
  case 0x6e: c->l = m8080_rb(c, c->hl); break; // MOV L,M
  case 0x6f: c->l = c->a; break; // MOV L,A
  case 0x70: m8080_wb(c, c->hl, c->b); break; // MOV M,B
  case 0x71: m8080_wb(c, c->hl, c->c); break; // MOV M,C
  case 0x72: m8080_wb(c, c->hl, c->d); break; // MOV M,D
  case 0x73: m8080_wb(c, c->hl, c->e); break; // MOV M,E
  case 0x74: m8080_wb(c, c->hl, c->h); break; // MOV M,H
  case 0x75: m8080_wb(c, c->hl, c->l); break; // MOV M,L
  case 0x77: m8080_wb(c, c->hl, c->a); break; // MOV M,A
  case 0x78: c->a = c->b; break; // MOV A,B
  case 0x79: c->a = c->c; break; // MOV A,C
  case 0x7a: c->a = c->d; break; // MOV A,D
  case 0x7b: c->a = c->e; break; // MOV A,E
  case 0x7c: c->a = c->h; break; // MOV A,H
  case 0x7d: c->a = c->l; break; // MOV A,L
  case 0x7e: c->a = m8080_rb(c, c->hl); break; // MOV A,M
  case 0x7f: c->a = c->a; break; // MOV A,A

  // store accumulator
  case 0x02: m8080_wb(c, c->bc, c->a); break; // STAX B
  case 0x12: m8080_wb(c, c->de, c->a); break; // STAX D

  // load accumulator
  case 0x0a: c->a = m8080_rb(c, c->bc); break; // LDAX B
  case 0x1a: c->a = m8080_rb(c, c->de); break; // LDAX D

  // add register or memory to accumulator
  case 0x80: m8080_add(c, c->b); break; // ADD B
  case 0x81: m8080_add(c, c->c); break; // ADD C
  case 0x82: m8080_add(c, c->d); break; // ADD D
  case 0x83: m8080_add(c, c->e); break; // ADD E
  case 0x84: m8080_add(c, c->h); break; // ADD H
  case 0x85: m8080_add(c, c->l); break; // ADD L
  case 0x86: m8080_add(c, m8080_rb(c, c->hl)); break; // ADD M
  case 0x87: m8080_add(c, c->a); break; // ADD A

  // add register or memory to accumulator with carry
  case 0x88: m8080_adc(c, c->b); break; // ADC B
  case 0x89: m8080_adc(c, c->c); break; // ADC C
  case 0x8a: m8080_adc(c, c->d); break; // ADC D
  case 0x8b: m8080_adc(c, c->e); break; // ADC E
  case 0x8c: m8080_adc(c, c->h); break; // ADC H
  case 0x8d: m8080_adc(c, c->l); break; // ADC L
  case 0x8e: m8080_adc(c, m8080_rb(c, c->hl)); break; // ADC M
  case 0x8f: m8080_adc(c, c->a); break; // ADC A

  // subtract register or memory from accumulator
  case 0x90: m8080_sub(c, c->b); break; // SUB B
  case 0x91: m8080_sub(c, c->c); break; // SUB C
  case 0x92: m8080_sub(c, c->d); break; // SUB D
  case 0x93: m8080_sub(c, c->e); break; // SUB E
  case 0x94: m8080_sub(c, c->h); break; // SUB H
  case 0x95: m8080_sub(c, c->l); break; // SUB L
  case 0x96: m8080_sub(c, m8080_rb(c, c->hl)); break; // SUB M
  case 0x97: m8080_sub(c, c->a); break; // SUB A

  // subtract register or memory from accumulator with borrow
  case 0x98: m8080_sbb(c, c->b); break; // SBB B
  case 0x99: m8080_sbb(c, c->c); break; // SBB C
  case 0x9a: m8080_sbb(c, c->d); break; // SBB D
  case 0x9b: m8080_sbb(c, c->e); break; // SBB E
  case 0x9c: m8080_sbb(c, c->h); break; // SBB H
  case 0x9d: m8080_sbb(c, c->l); break; // SBB L
  case 0x9e: m8080_sbb(c, m8080_rb(c, c->hl)); break; // SBB M
  case 0x9f: m8080_sbb(c, c->a); break; // SBB A

  // logical AND register or memory with accumulator
  case 0xa0: m8080_ana(c, c->b); break; // ANA B
  case 0xa1: m8080_ana(c, c->c); break; // ANA C
  case 0xa2: m8080_ana(c, c->d); break; // ANA D
  case 0xa3: m8080_ana(c, c->e); break; // ANA E
  case 0xa4: m8080_ana(c, c->h); break; // ANA H
  case 0xa5: m8080_ana(c, c->l); break; // ANA L
  case 0xa6: m8080_ana(c, m8080_rb(c, c->hl)); break; // ANA M
  case 0xa7: m8080_ana(c, c->a); break; // ANA A

  // logical XOR register or memory with accumulator
  case 0xa8: m8080_xra(c, c->b); break; // XRA B
  case 0xa9: m8080_xra(c, c->c); break; // XRA C
  case 0xaa: m8080_xra(c, c->d); break; // XRA D
  case 0xab: m8080_xra(c, c->e); break; // XRA E
  case 0xac: m8080_xra(c, c->h); break; // XRA H
  case 0xad: m8080_xra(c, c->l); break; // XRA L
  case 0xae: m8080_xra(c, m8080_rb(c, c->hl)); break; // XRA M
  case 0xaf: m8080_xra(c, c->a); break; // XRA A

  // logical OR register or memory with accumulator
  case 0xb0: m8080_ora(c, c->b); break; // ORA B
  case 0xb1: m8080_ora(c, c->c); break; // ORA C
  case 0xb2: m8080_ora(c, c->d); break; // ORA D
  case 0xb3: m8080_ora(c, c->e); break; // ORA E
  case 0xb4: m8080_ora(c, c->h); break; // ORA H
  case 0xb5: m8080_ora(c, c->l); break; // ORA L
  case 0xb6: m8080_ora(c, m8080_rb(c, c->hl)); break; // ORA M
  case 0xb7: m8080_ora(c, c->a); break; // ORA A

  // compare register or memory with accumulator
  case 0xb8: m8080_cmp(c, c->b); break; // CMP B
  case 0xb9: m8080_cmp(c, c->c); break; // CMP C
  case 0xba: m8080_cmp(c, c->d); break; // CMP D
  case 0xbb: m8080_cmp(c, c->e); break; // CMP E
  case 0xbc: m8080_cmp(c, c->h); break; // CMP H
  case 0xbd: m8080_cmp(c, c->l); break; // CMP L
  case 0xbe: m8080_cmp(c, m8080_rb(c, c->hl)); break; // CMP M
  case 0xbf: m8080_cmp(c, c->a); break; // CMP A

  // rotate accumulator instructions
  case 0x07: m8080_rlc(c); break; // RLC
  case 0x0f: m8080_rrc(c); break; // RRC
  case 0x17: m8080_ral(c); break; // RAL
  case 0x1f: m8080_rar(c); break; // RAR

  // push data onto stack
  case 0xc5: m8080_push(c, c->bc); break; // PUSH B
  case 0xd5: m8080_push(c, c->de); break; // PUSH D
  case 0xe5: m8080_push(c, c->hl); break; // PUSH H
  case 0xf5: m8080_push_psw(c); break; // PUSH PSW

  // pop data off stack
  case 0xc1: c->bc = m8080_pop(c); break; // POP B
  case 0xd1: c->de = m8080_pop(c); break; // POP D
  case 0xe1: c->hl = m8080_pop(c); break; // POP H
  case 0xf1: m8080_pop_psw(c); break; // POP PSW

  // double add
  case 0x09: c->f.c = (c->hl + c->bc) >> 16; c->hl += c->bc; break; // DAD B
  case 0x19: c->f.c = (c->hl + c->de) >> 16; c->hl += c->de; break; // DAD D
  case 0x29: c->f.c = (c->hl + c->hl) >> 16; c->hl += c->hl; break; // DAD H
  case 0x39: c->f.c = (c->hl + c->sp) >> 16; c->hl += c->sp; break; // DAD SP

  // increment register pair
  case 0x03: ++c->bc; break; // INX B
  case 0x13: ++c->de; break; // INX D
  case 0x23: ++c->hl; break; // INX H
  case 0x33: ++c->sp; break; // INX SP

  // decrement register pair
  case 0x0b: --c->bc; break; // DCX B
  case 0x1b: --c->de; break; // DCX D
  case 0x2b: --c->hl; break; // DCX H
  case 0x3b: --c->sp; break; // DCX SP

  // exchange registers
  case 0xeb: m8080_xchg(c); break; // XCHG
  case 0xe3: m8080_xthl(c); break; // XTHL
  case 0xf9: c->sp = c->hl; break; // SPHL

  // move immediate word
  case 0x01: c->bc = m8080_next_word(c); break; // LXI B,word
  case 0x11: c->de = m8080_next_word(c); break; // LXI D,word
  case 0x21: c->hl = m8080_next_word(c); break; // LXI H,word
  case 0x31: c->sp = m8080_next_word(c); break; // LXI SP,word

  // move immediate byte
  case 0x06: c->b = m8080_next_byte(c); break; // MVI B,byte
  case 0x0e: c->c = m8080_next_byte(c); break; // MVI C,byte
  case 0x16: c->d = m8080_next_byte(c); break; // MVI D,byte
  case 0x1e: c->e = m8080_next_byte(c); break; // MVI E,byte
  case 0x26: c->h = m8080_next_byte(c); break; // MVI H,byte
  case 0x2e: c->l = m8080_next_byte(c); break; // MVI L,byte
  case 0x36: m8080_wb(c, c->hl, m8080_next_byte(c)); break; // MVI M,byte
  case 0x3e: c->a = m8080_next_byte(c); break; // MVI A,byte

  // immediate instructions
  case 0xc6: m8080_add(c, m8080_next_byte(c)); break; // ADI byte
  case 0xce: m8080_adc(c, m8080_next_byte(c)); break; // ACI byte
  case 0xd6: m8080_sub(c, m8080_next_byte(c)); break; // SUI byte
  case 0xde: m8080_sbb(c, m8080_next_byte(c)); break; // SBI byte
  case 0xe6: m8080_ana(c, m8080_next_byte(c)); break; // ANI byte
  case 0xee: m8080_xra(c, m8080_next_byte(c)); break; // XRI byte
  case 0xf6: m8080_ora(c, m8080_next_byte(c)); break; // ORI byte
  case 0xfe: m8080_cmp(c, m8080_next_byte(c)); break; // CPI byte

  // store/load accumulator direct
  case 0x32: m8080_wb(c, m8080_next_word(c), c->a); break; // STA word
  case 0x3a: c->a = m8080_rb(c, m8080_next_word(c)); break; // LDA word

  // store/load HL direct
  case 0x22: m8080_ww(c, m8080_next_word(c), c->hl); break; // SHLD word
  case 0x2a: c->hl = m8080_rw(c, m8080_next_word(c)); break; // LHLD word

  // load program counter
  case 0xe9: c->pc = c->hl; break; // PCHL

  // jump instructions
  case 0xc3: c->pc = m8080_next_word(c); break; // JMP word
  case 0xcb: c->pc = m8080_next_word(c); break; // JMP word
  case 0xda: m8080_cond_jmp(c, c->f.c == 1); break; // JC word
  case 0xd2: m8080_cond_jmp(c, c->f.c == 0); break; // JNC word
  case 0xca: m8080_cond_jmp(c, c->f.z == 1); break; // JZ word
  case 0xc2: m8080_cond_jmp(c, c->f.z == 0); break; // JNZ word
  case 0xfa: m8080_cond_jmp(c, c->f.s == 1); break; // JM word
  case 0xf2: m8080_cond_jmp(c, c->f.s == 0); break; // JP word
  case 0xea: m8080_cond_jmp(c, c->f.p == 1); break; // JPE word
  case 0xe2: m8080_cond_jmp(c, c->f.p == 0); break; // JPO word

  // call subroutine instructions
  case 0xcd: m8080_call(c, m8080_next_word(c)); break; // CALL word
  case 0xdd: m8080_call(c, m8080_next_word(c)); break; // CALL word
  case 0xed: m8080_call(c, m8080_next_word(c)); break; // CALL word
  case 0xfd: m8080_call(c, m8080_next_word(c)); break; // CALL word
  case 0xdc: m8080_cond_call(c, c->f.c == 1); break; // CC word
  case 0xd4: m8080_cond_call(c, c->f.c == 0); break; // CNC word
  case 0xcc: m8080_cond_call(c, c->f.z == 1); break; // CZ word
  case 0xc4: m8080_cond_call(c, c->f.z == 0); break; // CNZ word
  case 0xfc: m8080_cond_call(c, c->f.s == 1); break; // CM word
  case 0xf4: m8080_cond_call(c, c->f.s == 0); break; // CP word
  case 0xec: m8080_cond_call(c, c->f.p == 1); break; // CPE word
  case 0xe4: m8080_cond_call(c, c->f.p == 0); break; // CPO word

  // return from subroutine instructions
  case 0xc9: c->pc = m8080_pop(c); break; // RET
  case 0xd9: c->pc = m8080_pop(c); break; // RET
  case 0xd8: m8080_cond_ret(c, c->f.c == 1); break; // RC
  case 0xd0: m8080_cond_ret(c, c->f.c == 0); break; // RNC
  case 0xc8: m8080_cond_ret(c, c->f.z == 1); break; // RZ
  case 0xc0: m8080_cond_ret(c, c->f.z == 0); break; // RNZ
  case 0xf8: m8080_cond_ret(c, c->f.s == 1); break; // RM
  case 0xf0: m8080_cond_ret(c, c->f.s == 0); break; // RP
  case 0xe8: m8080_cond_ret(c, c->f.p == 1); break; // RPE
  case 0xe0: m8080_cond_ret(c, c->f.p == 0); break; // RPO

  // restart instructions
  case 0xc7: m8080_call(c, 0x00); break; // RST 0
  case 0xcf: m8080_call(c, 0x08); break; // RST 1
  case 0xd7: m8080_call(c, 0x10); break; // RST 2
  case 0xdf: m8080_call(c, 0x18); break; // RST 3
  case 0xe7: m8080_call(c, 0x20); break; // RST 4
  case 0xef: m8080_call(c, 0x28); break; // RST 5
  case 0xf7: m8080_call(c, 0x30); break; // RST 6
  case 0xff: m8080_call(c, 0x38); break; // RST 7

  // interrupt flip-flop instructions
  case 0xfb: c->inte = 1; break; // EI
  case 0xf3: c->inte = 0; break; // DI

  // input/output instructions (user-defined)
  case 0xdb: m8080_in(c, m8080_next_byte(c)); break; // IN byte
  case 0xd3: m8080_out(c, m8080_next_byte(c)); break; // OUT byte

  // halt instruction (user-defined)
  case 0x76: m8080_hlt(c); break; // HLT
  }

  return c->cycles - previous_cycle;
}

size_t m8080_interrupt(m8080* const c, const uint16_t a) {
  const size_t previous_cycle = c->cycles;
  if(c->inte) {
    c->inte = 0;
    m8080_call(c, a);
    c->cycles += 11;
  }
  return c->cycles - previous_cycle;
}

#endif // M8080_IMPLEMENTATION

/*
MIT License
Copyright (c) 2019 Pedro Minicz

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
