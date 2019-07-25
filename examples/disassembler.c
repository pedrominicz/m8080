/* Copyright (c) 2019 Pedro Minicz */
#define M8080_IMPLEMENTATION
#include "m8080.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static const size_t length[] = {
  1, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, // 00..0f
  1, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, // 00..1f
  1, 3, 3, 1, 1, 1, 2, 1, 1, 1, 3, 1, 1, 1, 2, 1, // 20..2f
  1, 3, 3, 1, 1, 1, 2, 1, 1, 1, 3, 1, 1, 1, 2, 1, // 30..3f
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 40..4f
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 50..5f
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 60..6f
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 70..7f
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 80..8f
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 90..9f
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // a0..af
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // b0..bf
  1, 1, 3, 3, 3, 1, 2, 1, 1, 1, 3, 3, 3, 3, 2, 1, // c0..cf
  1, 1, 3, 2, 3, 1, 2, 1, 1, 1, 3, 2, 3, 3, 2, 1, // d0..df
  1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 3, 2, 1, // e0..ef
  1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 3, 2, 1, // f0..ff
};

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

void map(const m8080* const c, size_t pos, uint8_t* const memory_map) {
  while(pos < 0x10000) {
    if(memory_map[pos]) break;

    const size_t l = length[m8080_rb(c, pos)];

    if(l >= 3) memory_map[pos + 2] = 3;
    if(l >= 2) memory_map[pos + 1] = 2;
    if(l >= 1) memory_map[pos + 0] = 1;

    switch(m8080_rb(c, pos)) {
    case 0xc3: // jmp word
    case 0xcb: // jmp word (undocumented)
      if(m8080_rw(c, pos + 1) >= c->pc) {
        pos = m8080_rw(c, pos + 1);
        continue;
      }
      // fallthrough
    case 0xc9: // ret
    case 0xd9: // ret (undocumented)
    case 0x76: // hlt
      return;

    case 0xda: // jc word
    case 0xd2: // jnc word
    case 0xca: // jz word
    case 0xc2: // jnz word
    case 0xfa: // jm word
    case 0xf2: // jp word
    case 0xea: // jpe word
    case 0xe2: // jpo word
    case 0xcd: // call word
    case 0xdd: // call word (undocumented)
    case 0xed: // call word (undocumented)
    case 0xfd: // call word (undocumented)
    case 0xdc: // cc word
    case 0xd4: // cnc word
    case 0xcc: // cz word
    case 0xc4: // cnz word
    case 0xfc: // cm word
    case 0xf4: // cp word
    case 0xec: // cpe word
    case 0xe4: // cpo word
      if(m8080_rw(c, pos + 1) >= c->pc)
        map(c, m8080_rw(c, pos + 1), memory_map);
      break;
    }

    pos += l;
  }
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

  uint8_t memory_map[0x10000] = {0};
  map(&c, c.pc, memory_map);

  size_t next = c.pc;
  for(size_t i = c.pc; i < 0x10000; ++i) {
    if(memory_map[i] == 1) {
      if(i < next) puts("warning: misaligned instructions!");
      if(i > next) puts("...");
      next = i + m8080_disassemble(&c, i, false);
      putchar('\n');
    }
  }

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
