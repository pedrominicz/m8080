/* Copyright (c) 2019 Pedro Minicz */
#define M8080_IMPLEMENTATION
#include "m8080.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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
  c->pc = 0;
}

static inline size_t test(const char* const file) {
  puts(file);

  m8080 c = {0};
  uint8_t memory[0x10000] = {0};
  c.userdata = memory;
  c.pc = 0x0100; // the test ROMs expect to be loaded at 0x0100

  FILE* f = fopen(file, "rb");
  if(!f) return 0;
  fread(memory + c.pc, 1, sizeof(memory) - c.pc, f);
  fclose(f);

  // the test ROMs expect a print function to be located at 0x0005
  memory[0x0005] = 0xc9; // RET

  while(1) {
    const uint16_t previous_pc = c.pc;
    m8080_step(&c);

    // handle CALL 0005
    if(c.pc == 0x0005) {
      // prints memory from DE until '$' is found
      if(c.c == 0x09) {
        for(uint16_t i = c.de; m8080_rb(&c, i) != '$'; ++i) {
          putchar(m8080_rb(&c, i));
        }
      }
      if(c.c == 0x02) putchar(c.e);
    }
    if(c.pc == 0) {
      printf("\njumped to 0000 from %04x (%zu cycles)\n\n", previous_pc, c.cycles);
      return c.cycles;
    }
  }
}

int main(int argc, char** argv) {
  size_t cycles = 0;
  cycles += test("roms/TST8080.COM");
  cycles += test("roms/CPUTEST.COM");
  cycles += test("roms/8080PRE.COM");
  cycles += test("roms/8080EXER.COM");
  printf("total cycles: %zu\n", cycles);

  return 0;
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
