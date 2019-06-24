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

#include <stdbool.h>
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

// prints the instruction at `pos` to stdout and returns it's size
// set `b` if there is a breakpoint at `pos`
// doesn't print an end of line
int m8080_disassemble(const m8080* const c, const uint16_t pos, const bool b);

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

int m8080_disassemble(const m8080* const c, const uint16_t pos, const bool b) {
  const uint8_t opcode = m8080_rb(c, pos);
  const uint8_t byte = m8080_rb(c, pos + 1);
  const uint16_t word = m8080_rw(c, pos + 1);
  printf("| 0x%04x %c\t", pos, b ? 'b' : ' ');

#define I1(s) printf("%02x        " s, opcode); return 1;
#define I2(s) printf("%02x%02x      " s, opcode, m8080_rb(c, pos + 1), byte); return 2;
#define I3(s) printf("%02x%02x%02x    " s, opcode, m8080_rb(c, pos + 1), m8080_rb(c, pos + 2), word); return 3;
  switch(opcode) {
  // set carry
  case 0x37: I1("stc");

  // complement carry
  case 0x3f: I1("cmc");

  // increment register or memory
  case 0x04: I1("inr b");
  case 0x0c: I1("inr c");
  case 0x14: I1("inr d");
  case 0x1c: I1("inr e");
  case 0x24: I1("inr h");
  case 0x2c: I1("inr l");
  case 0x34: I1("inr [hl]");
  case 0x3c: I1("inr a");

  // decrement register or memory
  case 0x05: I1("dcr b");
  case 0x0d: I1("dcr c");
  case 0x15: I1("dcr d");
  case 0x1d: I1("dcr e");
  case 0x25: I1("dcr h");
  case 0x2d: I1("dcr l");
  case 0x35: I1("dcr [hl]");
  case 0x3d: I1("dcr a");

  // complement accumulator
  case 0x2f: I1("cma");

  // decimal adjust accumulator
  case 0x27: I1("daa");

  // no operation instructions
  case 0x00: I1("nop");
  case 0x08: I1("nop"); // undocumented
  case 0x10: I1("nop"); // undocumented
  case 0x18: I1("nop"); // undocumented
  case 0x20: I1("nop"); // undocumented
  case 0x28: I1("nop"); // undocumented
  case 0x30: I1("nop"); // undocumented
  case 0x38: I1("nop"); // undocumented

  // move
  case 0x40: I1("mov b, b");
  case 0x41: I1("mov b, c");
  case 0x42: I1("mov b, d");
  case 0x43: I1("mov b, e");
  case 0x44: I1("mov b, h");
  case 0x45: I1("mov b, l");
  case 0x46: I1("mov b, [hl]");
  case 0x47: I1("mov b, a");
  case 0x48: I1("mov c, b");
  case 0x49: I1("mov c, c");
  case 0x4a: I1("mov c, d");
  case 0x4b: I1("mov c, e");
  case 0x4c: I1("mov c, h");
  case 0x4d: I1("mov c, l");
  case 0x4e: I1("mov c, [hl]");
  case 0x4f: I1("mov c, a");
  case 0x50: I1("mov d, b");
  case 0x51: I1("mov d, c");
  case 0x52: I1("mov d, d");
  case 0x53: I1("mov d, e");
  case 0x54: I1("mov d, h");
  case 0x55: I1("mov d, l");
  case 0x56: I1("mov d, [hl]");
  case 0x57: I1("mov d, a");
  case 0x58: I1("mov e, b");
  case 0x59: I1("mov e, c");
  case 0x5a: I1("mov e, d");
  case 0x5b: I1("mov e, e");
  case 0x5c: I1("mov e, h");
  case 0x5d: I1("mov e, l");
  case 0x5e: I1("mov e, [hl]");
  case 0x5f: I1("mov e, a");
  case 0x60: I1("mov h, b");
  case 0x61: I1("mov h, c");
  case 0x62: I1("mov h, d");
  case 0x63: I1("mov h, e");
  case 0x64: I1("mov h, h");
  case 0x65: I1("mov h, l");
  case 0x66: I1("mov h, [hl]");
  case 0x67: I1("mov h, a");
  case 0x68: I1("mov l, b");
  case 0x69: I1("mov l, c");
  case 0x6a: I1("mov l, d");
  case 0x6b: I1("mov l, e");
  case 0x6c: I1("mov l, h");
  case 0x6d: I1("mov l, l");
  case 0x6e: I1("mov l, [hl]");
  case 0x6f: I1("mov l, a");
  case 0x70: I1("mov [hl], b");
  case 0x71: I1("mov [hl], c");
  case 0x72: I1("mov [hl], d");
  case 0x73: I1("mov [hl], e");
  case 0x74: I1("mov [hl], h");
  case 0x75: I1("mov [hl], l");
  case 0x77: I1("mov [hl], a");
  case 0x78: I1("mov a, b");
  case 0x79: I1("mov a, c");
  case 0x7a: I1("mov a, d");
  case 0x7b: I1("mov a, e");
  case 0x7c: I1("mov a, h");
  case 0x7d: I1("mov a, l");
  case 0x7e: I1("mov a, [hl]");
  case 0x7f: I1("mov a, a");

  // store accumulator
  case 0x02: I1("stax b");
  case 0x12: I1("stax d");

  // load accumulator
  case 0x0a: I1("ldax b");
  case 0x1a: I1("ldax d");

  // add register or memory to accumulator
  case 0x80: I1("add b");
  case 0x81: I1("add c");
  case 0x82: I1("add d");
  case 0x83: I1("add e");
  case 0x84: I1("add h");
  case 0x85: I1("add l");
  case 0x86: I1("add [hl]");
  case 0x87: I1("add a");

  // add register or memory to accumulator with carry
  case 0x88: I1("adc b");
  case 0x89: I1("adc c");
  case 0x8a: I1("adc d");
  case 0x8b: I1("adc e");
  case 0x8c: I1("adc h");
  case 0x8d: I1("adc l");
  case 0x8e: I1("adc [hl]");
  case 0x8f: I1("adc a");

  // subtract register or memory from accumulator
  case 0x90: I1("sub b");
  case 0x91: I1("sub c");
  case 0x92: I1("sub d");
  case 0x93: I1("sub e");
  case 0x94: I1("sub h");
  case 0x95: I1("sub l");
  case 0x96: I1("sub [hl]");
  case 0x97: I1("sub a");

  // subtract register or memory from accumulator with borrow
  case 0x98: I1("sbb b");
  case 0x99: I1("sbb c");
  case 0x9a: I1("sbb d");
  case 0x9b: I1("sbb e");
  case 0x9c: I1("sbb h");
  case 0x9d: I1("sbb l");
  case 0x9e: I1("sbb [hl]");
  case 0x9f: I1("sbb a");

  // logical AND register or memory with accumulator
  case 0xa0: I1("ana b");
  case 0xa1: I1("ana c");
  case 0xa2: I1("ana d");
  case 0xa3: I1("ana e");
  case 0xa4: I1("ana h");
  case 0xa5: I1("ana l");
  case 0xa6: I1("ana [hl]");
  case 0xa7: I1("ana a");

  // logical XOR register or memory with accumulator
  case 0xa8: I1("xra b");
  case 0xa9: I1("xra c");
  case 0xaa: I1("xra d");
  case 0xab: I1("xra e");
  case 0xac: I1("xra h");
  case 0xad: I1("xra l");
  case 0xae: I1("xra [hl]");
  case 0xaf: I1("xra a");

  // logical OR register or memory with accumulator
  case 0xb0: I1("ora b");
  case 0xb1: I1("ora c");
  case 0xb2: I1("ora d");
  case 0xb3: I1("ora e");
  case 0xb4: I1("ora h");
  case 0xb5: I1("ora l");
  case 0xb6: I1("ora [hl]");
  case 0xb7: I1("ora a");

  // compare register or memory with accumulator
  case 0xb8: I1("cmp b");
  case 0xb9: I1("cmp c");
  case 0xba: I1("cmp d");
  case 0xbb: I1("cmp e");
  case 0xbc: I1("cmp h");
  case 0xbd: I1("cmp l");
  case 0xbe: I1("cmp [hl]");
  case 0xbf: I1("cmp a");

  // rotate accumulator instructions
  case 0x07: I1("rlc");
  case 0x0f: I1("rrc");
  case 0x17: I1("ral");
  case 0x1f: I1("rar");

  // push data onto stack
  case 0xc5: I1("push b");
  case 0xd5: I1("push d");
  case 0xe5: I1("push h");
  case 0xf5: I1("push psw");

  // pop data off stack
  case 0xc1: I1("pop b");
  case 0xd1: I1("pop d");
  case 0xe1: I1("pop h");
  case 0xf1: I1("pop psw");

  // double add
  case 0x09: I1("dad b");
  case 0x19: I1("dad d");
  case 0x29: I1("dad h");
  case 0x39: I1("dad sp");

  // increment register pair
  case 0x03: I1("inx b");
  case 0x13: I1("inx d");
  case 0x23: I1("inx h");
  case 0x33: I1("inx sp");

  // decrement register pair
  case 0x0b: I1("dcx b");
  case 0x1b: I1("dcx d");
  case 0x2b: I1("dcx h");
  case 0x3b: I1("dcx sp");

  // exchange registers
  case 0xeb: I1("xchg");
  case 0xe3: I1("xthl");
  case 0xf9: I1("sphl");

  // move immediate word
  case 0x01: I3("lxi b, 0x%x");
  case 0x11: I3("lxi d, 0x%x");
  case 0x21: I3("lxi h, 0x%x");
  case 0x31: I3("lxi sp, 0x%x");

  // move immediate byte
  case 0x06: I2("mvi b, 0x%x");
  case 0x0e: I2("mvi c, 0x%x");
  case 0x16: I2("mvi d, 0x%x");
  case 0x1e: I2("mvi e, 0x%x");
  case 0x26: I2("mvi h, 0x%x");
  case 0x2e: I2("mvi l, 0x%x");
  case 0x36: I2("mvi [hl], 0x%x");
  case 0x3e: I2("mvi a, 0x%x");

  // immediate instructions
  case 0xc6: I2("adi 0x%x");
  case 0xce: I2("aci 0x%x");
  case 0xd6: I2("sui 0x%x");
  case 0xde: I2("sbi 0x%x");
  case 0xe6: I2("ani 0x%x");
  case 0xee: I2("xri 0x%x");
  case 0xf6: I2("ori 0x%x");
  case 0xfe: I2("cpi 0x%x");

  // store/load accumulator direct
  case 0x32: I3("sta 0x%x");
  case 0x3a: I3("lda 0x%x");

  // store/load HL direct
  case 0x22: I3("shld 0x%x");
  case 0x2a: I3("lhld 0x%x");

  // load program counter
  case 0xe9: I1("pchl");

  // jump instructions
  case 0xc3: I3("jmp 0x%x");
  case 0xcb: I3("jmp 0x%x"); // undocumented
  case 0xda: I3("jc 0x%x");
  case 0xd2: I3("jnc 0x%x");
  case 0xca: I3("jz 0x%x");
  case 0xc2: I3("jnz 0x%x");
  case 0xfa: I3("jm 0x%x");
  case 0xf2: I3("jp 0x%x");
  case 0xea: I3("jpe 0x%x");
  case 0xe2: I3("jpo 0x%x");

  // call subroutine instructions
  case 0xcd: I3("call 0x%x");
  case 0xdd: I3("call 0x%x"); // undocumented
  case 0xed: I3("call 0x%x"); // undocumented
  case 0xfd: I3("call 0x%x"); // undocumented
  case 0xdc: I3("cc 0x%x");
  case 0xd4: I3("cnc 0x%x");
  case 0xcc: I3("cz 0x%x");
  case 0xc4: I3("cnz 0x%x");
  case 0xfc: I3("cm 0x%x");
  case 0xf4: I3("cp 0x%x");
  case 0xec: I3("cpe 0x%x");
  case 0xe4: I3("cpo 0x%x");

  // return from subroutine instructions
  case 0xc9: I1("ret");
  case 0xd9: I1("ret"); // undocumented
  case 0xd8: I1("rc");
  case 0xd0: I1("rnc");
  case 0xc8: I1("rz");
  case 0xc0: I1("rnz");
  case 0xf8: I1("rm");
  case 0xf0: I1("rp");
  case 0xe8: I1("rpe");
  case 0xe0: I1("rpo");

  // restart instructions
  case 0xc7: I1("rst 0");
  case 0xcf: I1("rst 1");
  case 0xd7: I1("rst 2");
  case 0xdf: I1("rst 3");
  case 0xe7: I1("rst 4");
  case 0xef: I1("rst 5");
  case 0xf7: I1("rst 6");
  case 0xff: I1("rst 7");

  // interrupt flip-flop instructions
  case 0xfb: I1("ei");
  case 0xf3: I1("di");

  // input/output instructions
  case 0xdb: I2("in 0x%x");
  case 0xd3: I2("out 0x%x");

  // halt instruction
  case 0x76: I1("hlt");
  }
#undef I1
#undef I2
#undef I3
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
  case 0x37: c->f.c = 1; break; // stc

  // complement carry
  case 0x3f: c->f.c = !c->f.c; break; // cmc

  // increment register or memory
  case 0x04: ++c->b; c->f.a = (c->b & 0x0f) == 0; m8080_set_pzs(c, c->b); break; // inr b
  case 0x0c: ++c->c; c->f.a = (c->c & 0x0f) == 0; m8080_set_pzs(c, c->c); break; // inr c
  case 0x14: ++c->d; c->f.a = (c->d & 0x0f) == 0; m8080_set_pzs(c, c->d); break; // inr d
  case 0x1c: ++c->e; c->f.a = (c->e & 0x0f) == 0; m8080_set_pzs(c, c->e); break; // inr e
  case 0x24: ++c->h; c->f.a = (c->h & 0x0f) == 0; m8080_set_pzs(c, c->h); break; // inr h
  case 0x2c: ++c->l; c->f.a = (c->l & 0x0f) == 0; m8080_set_pzs(c, c->l); break; // inr l
  case 0x34: { // inr [hl]
    const uint8_t res = m8080_rb(c, c->hl) + 1;
    m8080_wb(c, c->hl, res);
    c->f.a = (res & 0x0f) == 0;
    m8080_set_pzs(c, res);
  } break;
  case 0x3c: ++c->a; c->f.a = (c->a & 0x0f) == 0; m8080_set_pzs(c, c->a); break; // inr a

  // decrement register or memory
  case 0x05: --c->b; c->f.a = (c->b & 0x0f) != 0x0f; m8080_set_pzs(c, c->b); break; // dcr b
  case 0x0d: --c->c; c->f.a = (c->c & 0x0f) != 0x0f; m8080_set_pzs(c, c->c); break; // dcr c
  case 0x15: --c->d; c->f.a = (c->d & 0x0f) != 0x0f; m8080_set_pzs(c, c->d); break; // dcr d
  case 0x1d: --c->e; c->f.a = (c->e & 0x0f) != 0x0f; m8080_set_pzs(c, c->e); break; // dcr e
  case 0x25: --c->h; c->f.a = (c->h & 0x0f) != 0x0f; m8080_set_pzs(c, c->h); break; // dcr h
  case 0x2d: --c->l; c->f.a = (c->l & 0x0f) != 0x0f; m8080_set_pzs(c, c->l); break; // dcr l
  case 0x35: { // dcr [hl]
    const uint8_t res = m8080_rb(c, c->hl) - 1;
    m8080_wb(c, c->hl, res);
    c->f.a = (res & 0x0f) != 0x0f;
    m8080_set_pzs(c, res);
  } break;
  case 0x3d: --c->a; c->f.a = (c->a & 0x0f) != 0x0f; m8080_set_pzs(c, c->a); break; // dcr a

  // complement accumulator
  case 0x2f: c->a = ~c->a; break; // cma

  // decimal adjust accumulator
  case 0x27: m8080_daa(c); break; // daa

  // no operation instructions
  case 0x00: break; // nop
  case 0x08: break; // nop
  case 0x10: break; // nop
  case 0x18: break; // nop
  case 0x20: break; // nop
  case 0x28: break; // nop
  case 0x30: break; // nop
  case 0x38: break; // nop

  // move
  case 0x40: c->b = c->b; break; // mov b, b
  case 0x41: c->b = c->c; break; // mov b, c
  case 0x42: c->b = c->d; break; // mov b, d
  case 0x43: c->b = c->e; break; // mov b, e
  case 0x44: c->b = c->h; break; // mov b, h
  case 0x45: c->b = c->l; break; // mov b, l
  case 0x46: c->b = m8080_rb(c, c->hl); break; // mov b, [hl]
  case 0x47: c->b = c->a; break; // mov b, a
  case 0x48: c->c = c->b; break; // mov c, b
  case 0x49: c->c = c->c; break; // mov c, c
  case 0x4a: c->c = c->d; break; // mov c, d
  case 0x4b: c->c = c->e; break; // mov c, e
  case 0x4c: c->c = c->h; break; // mov c, h
  case 0x4d: c->c = c->l; break; // mov c, l
  case 0x4e: c->c = m8080_rb(c, c->hl); break; // mov c, [hl]
  case 0x4f: c->c = c->a; break; // mov c, a
  case 0x50: c->d = c->b; break; // mov d, b
  case 0x51: c->d = c->c; break; // mov d, c
  case 0x52: c->d = c->d; break; // mov d, d
  case 0x53: c->d = c->e; break; // mov d, e
  case 0x54: c->d = c->h; break; // mov d, h
  case 0x55: c->d = c->l; break; // mov d, l
  case 0x56: c->d = m8080_rb(c, c->hl); break; // mov d, [hl]
  case 0x57: c->d = c->a; break; // mov d, a
  case 0x58: c->e = c->b; break; // mov e, b
  case 0x59: c->e = c->c; break; // mov e, c
  case 0x5a: c->e = c->d; break; // mov e, d
  case 0x5b: c->e = c->e; break; // mov e, e
  case 0x5c: c->e = c->h; break; // mov e, h
  case 0x5d: c->e = c->l; break; // mov e, l
  case 0x5e: c->e = m8080_rb(c, c->hl); break; // mov e, [hl]
  case 0x5f: c->e = c->a; break; // mov e, a
  case 0x60: c->h = c->b; break; // mov h, b
  case 0x61: c->h = c->c; break; // mov h, c
  case 0x62: c->h = c->d; break; // mov h, d
  case 0x63: c->h = c->e; break; // mov h, e
  case 0x64: c->h = c->h; break; // mov h, h
  case 0x65: c->h = c->l; break; // mov h, l
  case 0x66: c->h = m8080_rb(c, c->hl); break; // mov h, [hl]
  case 0x67: c->h = c->a; break; // mov h, a
  case 0x68: c->l = c->b; break; // mov l, b
  case 0x69: c->l = c->c; break; // mov l, c
  case 0x6a: c->l = c->d; break; // mov l, d
  case 0x6b: c->l = c->e; break; // mov l, e
  case 0x6c: c->l = c->h; break; // mov l, h
  case 0x6d: c->l = c->l; break; // mov l, l
  case 0x6e: c->l = m8080_rb(c, c->hl); break; // mov l, [hl]
  case 0x6f: c->l = c->a; break; // mov l, a
  case 0x70: m8080_wb(c, c->hl, c->b); break; // mov [hl], b
  case 0x71: m8080_wb(c, c->hl, c->c); break; // mov [hl], c
  case 0x72: m8080_wb(c, c->hl, c->d); break; // mov [hl], d
  case 0x73: m8080_wb(c, c->hl, c->e); break; // mov [hl], e
  case 0x74: m8080_wb(c, c->hl, c->h); break; // mov [hl], h
  case 0x75: m8080_wb(c, c->hl, c->l); break; // mov [hl], l
  case 0x77: m8080_wb(c, c->hl, c->a); break; // mov [hl], a
  case 0x78: c->a = c->b; break; // mov a, b
  case 0x79: c->a = c->c; break; // mov a, c
  case 0x7a: c->a = c->d; break; // mov a, d
  case 0x7b: c->a = c->e; break; // mov a, e
  case 0x7c: c->a = c->h; break; // mov a, h
  case 0x7d: c->a = c->l; break; // mov a, l
  case 0x7e: c->a = m8080_rb(c, c->hl); break; // mov a, [hl]
  case 0x7f: c->a = c->a; break; // mov a, a

  // store accumulator
  case 0x02: m8080_wb(c, c->bc, c->a); break; // stax b
  case 0x12: m8080_wb(c, c->de, c->a); break; // stax d

  // load accumulator
  case 0x0a: c->a = m8080_rb(c, c->bc); break; // ldax b
  case 0x1a: c->a = m8080_rb(c, c->de); break; // ldax d

  // add register or memory to accumulator
  case 0x80: m8080_add(c, c->b); break; // add b
  case 0x81: m8080_add(c, c->c); break; // add c
  case 0x82: m8080_add(c, c->d); break; // add d
  case 0x83: m8080_add(c, c->e); break; // add e
  case 0x84: m8080_add(c, c->h); break; // add h
  case 0x85: m8080_add(c, c->l); break; // add l
  case 0x86: m8080_add(c, m8080_rb(c, c->hl)); break; // add [hl]
  case 0x87: m8080_add(c, c->a); break; // add a

  // add register or memory to accumulator with carry
  case 0x88: m8080_adc(c, c->b); break; // adc b
  case 0x89: m8080_adc(c, c->c); break; // adc c
  case 0x8a: m8080_adc(c, c->d); break; // adc d
  case 0x8b: m8080_adc(c, c->e); break; // adc e
  case 0x8c: m8080_adc(c, c->h); break; // adc h
  case 0x8d: m8080_adc(c, c->l); break; // adc l
  case 0x8e: m8080_adc(c, m8080_rb(c, c->hl)); break; // adc [hl]
  case 0x8f: m8080_adc(c, c->a); break; // adc a

  // subtract register or memory from accumulator
  case 0x90: m8080_sub(c, c->b); break; // sub b
  case 0x91: m8080_sub(c, c->c); break; // sub c
  case 0x92: m8080_sub(c, c->d); break; // sub d
  case 0x93: m8080_sub(c, c->e); break; // sub e
  case 0x94: m8080_sub(c, c->h); break; // sub h
  case 0x95: m8080_sub(c, c->l); break; // sub l
  case 0x96: m8080_sub(c, m8080_rb(c, c->hl)); break; // sub [hl]
  case 0x97: m8080_sub(c, c->a); break; // sub a

  // subtract register or memory from accumulator with borrow
  case 0x98: m8080_sbb(c, c->b); break; // sbb b
  case 0x99: m8080_sbb(c, c->c); break; // sbb c
  case 0x9a: m8080_sbb(c, c->d); break; // sbb d
  case 0x9b: m8080_sbb(c, c->e); break; // sbb e
  case 0x9c: m8080_sbb(c, c->h); break; // sbb h
  case 0x9d: m8080_sbb(c, c->l); break; // sbb l
  case 0x9e: m8080_sbb(c, m8080_rb(c, c->hl)); break; // sbb [hl]
  case 0x9f: m8080_sbb(c, c->a); break; // sbb a

  // logical and register or memory with accumulator
  case 0xa0: m8080_ana(c, c->b); break; // ana b
  case 0xa1: m8080_ana(c, c->c); break; // ana c
  case 0xa2: m8080_ana(c, c->d); break; // ana d
  case 0xa3: m8080_ana(c, c->e); break; // ana e
  case 0xa4: m8080_ana(c, c->h); break; // ana h
  case 0xa5: m8080_ana(c, c->l); break; // ana l
  case 0xa6: m8080_ana(c, m8080_rb(c, c->hl)); break; // ana [hl]
  case 0xa7: m8080_ana(c, c->a); break; // ana a

  // logical xor register or memory with accumulator
  case 0xa8: m8080_xra(c, c->b); break; // xra b
  case 0xa9: m8080_xra(c, c->c); break; // xra c
  case 0xaa: m8080_xra(c, c->d); break; // xra d
  case 0xab: m8080_xra(c, c->e); break; // xra e
  case 0xac: m8080_xra(c, c->h); break; // xra h
  case 0xad: m8080_xra(c, c->l); break; // xra l
  case 0xae: m8080_xra(c, m8080_rb(c, c->hl)); break; // xra [hl]
  case 0xaf: m8080_xra(c, c->a); break; // xra a

  // logical OR register or memory with accumulator
  case 0xb0: m8080_ora(c, c->b); break; // ora b
  case 0xb1: m8080_ora(c, c->c); break; // ora c
  case 0xb2: m8080_ora(c, c->d); break; // ora d
  case 0xb3: m8080_ora(c, c->e); break; // ora e
  case 0xb4: m8080_ora(c, c->h); break; // ora h
  case 0xb5: m8080_ora(c, c->l); break; // ora l
  case 0xb6: m8080_ora(c, m8080_rb(c, c->hl)); break; // ora [hl]
  case 0xb7: m8080_ora(c, c->a); break; // ora a

  // compare register or memory with accumulator
  case 0xb8: m8080_cmp(c, c->b); break; // cmp b
  case 0xb9: m8080_cmp(c, c->c); break; // cmp c
  case 0xba: m8080_cmp(c, c->d); break; // cmp d
  case 0xbb: m8080_cmp(c, c->e); break; // cmp e
  case 0xbc: m8080_cmp(c, c->h); break; // cmp h
  case 0xbd: m8080_cmp(c, c->l); break; // cmp l
  case 0xbe: m8080_cmp(c, m8080_rb(c, c->hl)); break; // cmp [hl]
  case 0xbf: m8080_cmp(c, c->a); break; // cmp a

  // rotate accumulator instructions
  case 0x07: m8080_rlc(c); break; // rlc
  case 0x0f: m8080_rrc(c); break; // rrc
  case 0x17: m8080_ral(c); break; // ral
  case 0x1f: m8080_rar(c); break; // rar

  // push data onto stack
  case 0xc5: m8080_push(c, c->bc); break; // push b
  case 0xd5: m8080_push(c, c->de); break; // push d
  case 0xe5: m8080_push(c, c->hl); break; // push h
  case 0xf5: m8080_push_psw(c); break; // push psw

  // pop data off stack
  case 0xc1: c->bc = m8080_pop(c); break; // pop b
  case 0xd1: c->de = m8080_pop(c); break; // pop d
  case 0xe1: c->hl = m8080_pop(c); break; // pop h
  case 0xf1: m8080_pop_psw(c); break; // pop psw

  // double add
  case 0x09: c->f.c = (c->hl + c->bc) >> 16; c->hl += c->bc; break; // dad b
  case 0x19: c->f.c = (c->hl + c->de) >> 16; c->hl += c->de; break; // dad d
  case 0x29: c->f.c = (c->hl + c->hl) >> 16; c->hl += c->hl; break; // dad h
  case 0x39: c->f.c = (c->hl + c->sp) >> 16; c->hl += c->sp; break; // dad sp

  // increment register pair
  case 0x03: ++c->bc; break; // inx b
  case 0x13: ++c->de; break; // inx d
  case 0x23: ++c->hl; break; // inx h
  case 0x33: ++c->sp; break; // inx sp

  // decrement register pair
  case 0x0b: --c->bc; break; // dcx b
  case 0x1b: --c->de; break; // dcx d
  case 0x2b: --c->hl; break; // dcx h
  case 0x3b: --c->sp; break; // dcx sp

  // exchange registers
  case 0xeb: m8080_xchg(c); break; // xchg
  case 0xe3: m8080_xthl(c); break; // xthl
  case 0xf9: c->sp = c->hl; break; // sphl

  // move immediate word
  case 0x01: c->bc = m8080_next_word(c); break; // lxi b, word
  case 0x11: c->de = m8080_next_word(c); break; // lxi d, word
  case 0x21: c->hl = m8080_next_word(c); break; // lxi h, word
  case 0x31: c->sp = m8080_next_word(c); break; // lxi sp, word

  // move immediate byte
  case 0x06: c->b = m8080_next_byte(c); break; // mvi b, byte
  case 0x0e: c->c = m8080_next_byte(c); break; // mvi c, byte
  case 0x16: c->d = m8080_next_byte(c); break; // mvi d, byte
  case 0x1e: c->e = m8080_next_byte(c); break; // mvi e, byte
  case 0x26: c->h = m8080_next_byte(c); break; // mvi h, byte
  case 0x2e: c->l = m8080_next_byte(c); break; // mvi l, byte
  case 0x36: m8080_wb(c, c->hl, m8080_next_byte(c)); break; // mvi [hl], byte
  case 0x3e: c->a = m8080_next_byte(c); break; // mvi a, byte

  // immediate instructions
  case 0xc6: m8080_add(c, m8080_next_byte(c)); break; // adi byte
  case 0xce: m8080_adc(c, m8080_next_byte(c)); break; // aci byte
  case 0xd6: m8080_sub(c, m8080_next_byte(c)); break; // sui byte
  case 0xde: m8080_sbb(c, m8080_next_byte(c)); break; // sbi byte
  case 0xe6: m8080_ana(c, m8080_next_byte(c)); break; // ani byte
  case 0xee: m8080_xra(c, m8080_next_byte(c)); break; // xri byte
  case 0xf6: m8080_ora(c, m8080_next_byte(c)); break; // ori byte
  case 0xfe: m8080_cmp(c, m8080_next_byte(c)); break; // cpi byte

  // store/load accumulator direct
  case 0x32: m8080_wb(c, m8080_next_word(c), c->a); break; // sta word
  case 0x3a: c->a = m8080_rb(c, m8080_next_word(c)); break; // lda word

  // store/load hl direct
  case 0x22: m8080_ww(c, m8080_next_word(c), c->hl); break; // shld word
  case 0x2a: c->hl = m8080_rw(c, m8080_next_word(c)); break; // lhld word

  // load program counter
  case 0xe9: c->pc = c->hl; break; // pchl

  // jump instructions
  case 0xc3: c->pc = m8080_next_word(c); break; // jmp word
  case 0xcb: c->pc = m8080_next_word(c); break; // jmp word
  case 0xda: m8080_cond_jmp(c, c->f.c == 1); break; // jc word
  case 0xd2: m8080_cond_jmp(c, c->f.c == 0); break; // jnc word
  case 0xca: m8080_cond_jmp(c, c->f.z == 1); break; // jz word
  case 0xc2: m8080_cond_jmp(c, c->f.z == 0); break; // jnz word
  case 0xfa: m8080_cond_jmp(c, c->f.s == 1); break; // jm word
  case 0xf2: m8080_cond_jmp(c, c->f.s == 0); break; // jp word
  case 0xea: m8080_cond_jmp(c, c->f.p == 1); break; // jpe word
  case 0xe2: m8080_cond_jmp(c, c->f.p == 0); break; // jpo word

  // call subroutine instructions
  case 0xcd: m8080_call(c, m8080_next_word(c)); break; // call word
  case 0xdd: m8080_call(c, m8080_next_word(c)); break; // call word
  case 0xed: m8080_call(c, m8080_next_word(c)); break; // call word
  case 0xfd: m8080_call(c, m8080_next_word(c)); break; // call word
  case 0xdc: m8080_cond_call(c, c->f.c == 1); break; // cc word
  case 0xd4: m8080_cond_call(c, c->f.c == 0); break; // cnc word
  case 0xcc: m8080_cond_call(c, c->f.z == 1); break; // cz word
  case 0xc4: m8080_cond_call(c, c->f.z == 0); break; // cnz word
  case 0xfc: m8080_cond_call(c, c->f.s == 1); break; // cm word
  case 0xf4: m8080_cond_call(c, c->f.s == 0); break; // cp word
  case 0xec: m8080_cond_call(c, c->f.p == 1); break; // cpe word
  case 0xe4: m8080_cond_call(c, c->f.p == 0); break; // cpo word

  // return from subroutine instructions
  case 0xc9: c->pc = m8080_pop(c); break; // ret
  case 0xd9: c->pc = m8080_pop(c); break; // ret
  case 0xd8: m8080_cond_ret(c, c->f.c == 1); break; // rc
  case 0xd0: m8080_cond_ret(c, c->f.c == 0); break; // rnc
  case 0xc8: m8080_cond_ret(c, c->f.z == 1); break; // rz
  case 0xc0: m8080_cond_ret(c, c->f.z == 0); break; // rnz
  case 0xf8: m8080_cond_ret(c, c->f.s == 1); break; // rm
  case 0xf0: m8080_cond_ret(c, c->f.s == 0); break; // rp
  case 0xe8: m8080_cond_ret(c, c->f.p == 1); break; // rpe
  case 0xe0: m8080_cond_ret(c, c->f.p == 0); break; // rpo

  // restart instructions
  case 0xc7: m8080_call(c, M8080_RST_0); break; // rst 0
  case 0xcf: m8080_call(c, M8080_RST_1); break; // rst 1
  case 0xd7: m8080_call(c, M8080_RST_2); break; // rst 2
  case 0xdf: m8080_call(c, M8080_RST_3); break; // rst 3
  case 0xe7: m8080_call(c, M8080_RST_4); break; // rst 4
  case 0xef: m8080_call(c, M8080_RST_5); break; // rst 5
  case 0xf7: m8080_call(c, M8080_RST_6); break; // rst 6
  case 0xff: m8080_call(c, M8080_RST_7); break; // rst 7

  // interrupt flip-flop instructions
  case 0xfb: c->inte = 1; break; // ei
  case 0xf3: c->inte = 0; break; // di

  // input/output instructions (user-defined)
  case 0xdb: m8080_in(c, m8080_next_byte(c)); break; // in byte
  case 0xd3: m8080_out(c, m8080_next_byte(c)); break; // out byte

  // halt instruction (user-defined)
  case 0x76: m8080_hlt(c); break; // hlt
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
