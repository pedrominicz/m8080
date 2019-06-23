/* Copyright (c) 2019 Pedro Minicz */
#define M8080_IMPLEMENTATION
#include "m8080.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
  NOP = 0,
  BREAK,
  CONTINUE,
  DISASSEMBLE,
  DISASSEMBLE_FUNCTION,
  PRINT_REGISTERS,
  QUIT,
  STEP,
  HELP,
};

typedef struct Command {
  int type;
  int data;
} Command;

uint8_t m8080_rb(const m8080* const c, const uint16_t a) {
  const uint8_t* const memory = c->userdata;
  return memory[a];
}

void m8080_wb(m8080* const c, const uint16_t a, const uint8_t b) {
  uint8_t* const memory = c->userdata;
  memory[a] = b;
}

void m8080_in(m8080* const c, const uint8_t a) { }
void m8080_out(m8080* const c, const uint8_t a) { }

void m8080_hlt(m8080* const c) {
  --c->pc;
}

static inline int read_argument(void) {
  int ret = -1;
  // read until optional argument or end of line
  int ch = getchar();
  while(ch == ' ' || ch == '\t') ch = getchar();
  // no optional argument
  if(ch == '\n') {
    ungetc(ch, stdin);
    return 1;
  }
  // return first character of optional argument to `stdin`
  ungetc(ch, stdin);
  scanf("%i", &ret);
  return ret;
}

static inline Command read_command(void) {
  Command cmd = {0};

  int ch = getchar();
  while(ch == ' ' || ch == '\t') ch = getchar();

  switch(ch) {
  case '\n':
    cmd.type = NOP;
    return cmd;
  case 'b':
    ch = getchar();
    while(ch == ' ' || ch == '\t') ch = getchar();
    if(ch == '\n') { // breakpoint requires one argument
      cmd.type = HELP;
      return cmd;
    }
    ungetc(ch, stdin);
    cmd.data = read_argument();
    cmd.type = cmd.data != -1 ? BREAK : HELP;
    break;
  case 'c':
    cmd.type = CONTINUE;
    break;
  case 'd':
    cmd.data = read_argument();
    cmd.type = cmd.data != -1 ? DISASSEMBLE : HELP;
    break;
  case 'f':
    cmd.type = DISASSEMBLE_FUNCTION;
    break;
  case 'p':
    cmd.type = PRINT_REGISTERS;
    break;
  case EOF:
  case 'q':
    cmd.type = QUIT;
    break;
  case 's':
    cmd.data = read_argument();
    cmd.type = cmd.data != -1 ? STEP : HELP;
    break;
  case 'h':
  default:
    cmd.type = HELP;
    break;
  }

  // eat all characters until end of line
  while(1) {
    ch = getchar();
    if(ch == EOF || ch == '\n') return cmd;
    if(ch != ' ' && ch != '\t') cmd.type = HELP;
  }
}

static inline int debug_disassemble(const m8080* const c, const uint16_t pos, const bool b) {
  int ret = m8080_disassemble(c, pos, b);
  if(m8080_rb(c, pos) == 0xcd
      && m8080_rb(c, pos + 1) == 0x05
      && m8080_rb(c, pos + 2) == 0x00) {
    printf("\t; special print function");
  }
  putchar('\n');
  return ret;
}

static inline size_t debug_step(m8080* const c) {
  const size_t ret = m8080_step(c);

  // handle `call 0005`
  if(c->pc == 0x0005) {
    // prints memory from DE until '$' is found
    if(c->c == 0x09) {
      for(uint16_t i = c->de; m8080_rb(c, i) != '$'; ++i) {
        putchar(m8080_rb(c, i));
      }
    }
    if(c->c == 0x02) putchar(c->e);

    c->pc = m8080_rw(c, c->sp);
    c->sp += 2;
  }

  return ret;
}

static inline void print_registers(const m8080* const c) {
  uint8_t f = 0x02; // bit 1 is always 1, refer to `m8080_push_psw` for more info
  f |= c->f.c << 0;
  f |= c->f.p << 2;
  f |= c->f.a << 4;
  f |= c->f.z << 6;
  f |= c->f.s << 7;
  printf("    af   bc   de   hl   pc   sp  flags cycles\n");
  printf("0x %02x%02x %04x %04x %04x %04x %04x %c%c%c%c%c %zu\n",
      c->a, f, c->bc, c->de, c->hl, c->pc, c->sp,
      c->f.c ? 'c' : '.', c->f.p ? 'p' : '.', c->f.a ? 'a' : '.',
      c->f.z ? 'z' : '.', c->f.s ? 's' : '.', c->cycles);
}

int main(int argc, char** argv) {
  if(argc != 2) {
    fprintf(stderr, "usage: %s file\n", argv[0]);
    return 1;
  }

  m8080 c = {0};
  uint8_t memory[0x10000] = {0};
  c.userdata = memory;
  c.pc = 0x0100; // the test ROMs expect to be loaded at 0x0100

  FILE* f = fopen(argv[1], "rb");
  if(!f) {
    fprintf(stderr, "cannot open file: %s\n", argv[1]);
    return 1;
  }
  fread(memory + c.pc, 1, sizeof(memory) - c.pc, f);
  fclose(f);

  // the test ROMs jump to 0x0000 when finished
  memory[0x0000] = 0x76; // hlt

  bool breakpoint[0x10000] = {0};

  while(1) {
    printf("[0x%04x]> ", c.pc);
    const Command cmd = read_command();

    switch(cmd.type) {
    case NOP: break;
    case BREAK:
      breakpoint[cmd.data] = !breakpoint[cmd.data];
      printf("%s breakpoint at 0x%04x\n",
          breakpoint[cmd.data] ? "added" : "removed",
          cmd.data);
      break;
    case CONTINUE:
      while(memory[c.pc] != 0x76) {
        if(breakpoint[c.pc]) break;
        debug_step(&c);
      }
      break;
    case DISASSEMBLE: {
      size_t pos = c.pc;
      for(size_t i = 0; i < cmd.data; ++i) {
        if(pos >= 0x10000) {
          printf("EOF\n");
          break;
        }
        pos += debug_disassemble(&c, pos, breakpoint[pos]);
      }
    } break;
    case DISASSEMBLE_FUNCTION: {
      size_t pos = c.pc;
      for(size_t i = 0; i < 16; ++i) {
        if(pos >= 0x10000) {
          printf("EOF\n");
          break;
        }
        pos += debug_disassemble(&c, pos, breakpoint[pos]);
        if(memory[pos] == 0xc9 || memory[pos] == 0xd9) {
          debug_disassemble(&c, pos, breakpoint[pos]); // print `ret`
          break;
        }
      }
    } break;
    case PRINT_REGISTERS:
      print_registers(&c);
      break;
    case QUIT:
      printf("quit\n");
      return 0;
    case STEP:
      for(size_t i = 0; i < cmd.data; ++i) {
        debug_step(&c);
      }
      break;
    case HELP:
    default:
      printf("usage: [command] [option]\n");
      printf("| b [pos]   toggle breakpoint at pos\n");
      printf("| c         continue until breakpoint or halt\n");
      printf("| d         disassemble next instruction\n");
      printf("| d [count] disassemble count instructions\n");
      printf("| f         disassemble until return instruction\n");
      printf("| h         print this help message\n");
      printf("| p         print registers\n");
      printf("| s         step one instruction\n");
      printf("| s [count] step count instructions\n");
      break;
    }
  }
}

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
