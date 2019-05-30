/* Copyright (c) 2019 Pedro Minicz */
#define M8080_IMPLEMENTATION
#include "m8080.h"

#include <allegro5/allegro.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Invaders {
  uint8_t memory[0x10000];
  uint8_t in1; // input port 1
  // since the 8080 only includes instructions for bit shifting by one, space
  // invaders has bitshift hardware accessible on output ports 2 and 4 and
  // input port 3
  uint16_t shift;
  uint8_t shiftoffset;
} Invaders;

static ALLEGRO_BITMAP* bitmap;
static ALLEGRO_DISPLAY* display;
static ALLEGRO_EVENT_QUEUE* event_queue;
static ALLEGRO_TIMER* timer;

uint8_t m8080_rb(const m8080* const c, const uint16_t a) {
  const Invaders* const si = c->userdata;
  return si->memory[a];
}

void m8080_wb(m8080* const c, const uint16_t a, const uint8_t b) {
  Invaders* const si = c->userdata;
  // write outside RAM area
  if(a < 0x2000 || a > 0x3fff) return;
  si->memory[a] = b;
}

void m8080_in(m8080* const c, const uint8_t a) {
  Invaders* const si = c->userdata;
  switch(a) {
  case 1: c->a = si->in1; break;
  // reading port 3 returns the most significant eight bits of the shift
  // register shifted to the left by the offset
  case 3: c->a = si->shift >> (8 - si->shiftoffset); break;
  // other ports are not implemented, return zero
  default: c->a = 0;
  }
}

void m8080_out(m8080* const c, const uint8_t a) {
  Invaders* const si = c->userdata;
  switch(a) {
  // since the offset can be at most seven, only the least significant three
  // bits of the accumulator are taken into account
  case 2: si->shiftoffset = c->a & 0x07; break;
  // shifts the least significant byte of the shift register to the left by one
  // byte and adds the accumulator to the most significant byte
  case 4: si->shift = c->a << 8 | si->shift >> 8; break;
  }
}

// halt instruction
void m8080_hlt(m8080* const c) {
  exit(0);
}

static inline void invaders_init(Invaders* const si) {
  // bit 3 of input port 1 is always 1
  si->in1 = 0x08;
  // this space invaders emulator expects the ROM to be in a single file which
  // is simply a concatenation of the separate ROM files
  //
  //  $ cat invaders.{h,g,f,e} >invaders.rom
  FILE* f = fopen("roms/invaders.rom", "rb");
  if(!f) {
    puts("cannot open 'roms/invaders.rom'");
    exit(1);
  }
  const size_t len = fread(si->memory, 1, sizeof(si->memory), f);
  fclose(f);
  if(len != 8192) exit(1);
}

static inline void invaders_al_init(void) {
  if(!al_init()) exit(1);
  if(!al_install_keyboard()) exit(1);

  // force vsync off because space invaders timing relies on two screen
  // interrupts every frame
  al_set_new_display_option(ALLEGRO_VSYNC, 2, ALLEGRO_REQUIRE);
  display = al_create_display(224, 256);
  if(!display) exit(1);

  // timer for the screen interrupts
  timer = al_create_timer(1.0 / 120.0);
  if(!timer) exit(1);

  event_queue = al_create_event_queue();
  if(!event_queue) exit(1);
  al_register_event_source(event_queue, al_get_display_event_source(display));
  al_register_event_source(event_queue, al_get_keyboard_event_source());
  al_register_event_source(event_queue, al_get_timer_event_source(timer));

  // the bitmap has to be stored on system memory since invaders_draw() writes
  // directly to it every frame having it on GPU memory would be slow
  const int bitmap_flags = al_get_new_bitmap_flags()
    | ALLEGRO_MEMORY_BITMAP;
  al_set_new_bitmap_flags(bitmap_flags);
  bitmap = al_create_bitmap(224, 256);
  if(!bitmap) exit(1);
}

static inline void invaders_handle_keyboard(Invaders* const si, const ALLEGRO_EVENT ev) {
  // input port 1 mostly holds player 1 button presses, the format is:
  //
  //    bit 0 = credit
  //    bit 1 = player 2 start (not implemented)
  //    bit 2 = player 1 start
  //    bit 3 = always 1
  //    bit 4 = player 1 shoot
  //    bit 5 = player 1 left
  //    bit 6 = player 1 right
  //    bit 7 = always 0
  //
  // bit 3 is handled in invaders_init()
  if(ev.type == ALLEGRO_EVENT_KEY_DOWN) {
    switch(ev.keyboard.keycode) {
    case ALLEGRO_KEY_C: si->in1 |= 0x01; // coin
    case ALLEGRO_KEY_ENTER: si->in1 |= 0x04; break; // player 1 start
    case ALLEGRO_KEY_SPACE: si->in1 |= 0x10; break; // player 1 shoot
    case ALLEGRO_KEY_LEFT: si->in1 |= 0x20; break; // player 1 left
    case ALLEGRO_KEY_RIGHT: si->in1 |= 0x40; break; // player 1 right
    }
  }
  else if(ev.type == ALLEGRO_EVENT_KEY_UP) {
    switch(ev.keyboard.keycode) {
    case ALLEGRO_KEY_C: si->in1 &= ~0x01; // coin
    case ALLEGRO_KEY_ENTER: si->in1 &= ~0x04; break; // player 1 start
    case ALLEGRO_KEY_SPACE: si->in1 &= ~0x10; break; // player 1 shoot
    case ALLEGRO_KEY_LEFT: si->in1 &= ~0x20; break; // player 1 left
    case ALLEGRO_KEY_RIGHT: si->in1 &= ~0x40; break; // player 1 right
    }
  }
}

static inline void invaders_draw(const m8080* const c, ALLEGRO_BITMAP* const bitmap) {
  const ALLEGRO_COLOR green = al_map_rgb(0, 255, 0);
  const ALLEGRO_COLOR red = al_map_rgb(255, 0, 0);
  const ALLEGRO_COLOR white = al_map_rgb(255, 255, 255);

  al_set_target_bitmap(bitmap);
  al_clear_to_color(al_map_rgb(0, 0, 0));
  // the screen is 224 * 256 pixels with 8 pixels per byte
  for(size_t i = 0; i < 224 * 256 / 8; ++i) {
    // the screen in the cabinet is rotated 90 degrees counter-clockwise
    const int x = i / (256 / 8);
    const int y = 255 - (i * 8) % 256;

    //  +---------+
    //  |.........|
    //  |RRRRRRRRR|
    //  |.........|
    //  |.........|
    //  |.........|
    //  |GGGGGGGGG|
    //  |.GGGG....|
    //  +---------+
    //
    // red (R) and green (G) color overlay (remember that the origin is in the
    // top-left corner and that the Y-axis is reversed)
    ALLEGRO_COLOR color = white;
    if(y >= 32 && y < 64) color = red;
    if(y >= 184) {
      color = green;
      if(y >= 240 && (x < 16 || x >= 134)) color = white;
    }

    // video RAM is on 2400-3fff
    const uint8_t pixels = m8080_rb(c, 0x2400 + i);
    if(pixels & 0x01) al_put_pixel(x, y - 0, color);
    if(pixels & 0x02) al_put_pixel(x, y - 1, color);
    if(pixels & 0x04) al_put_pixel(x, y - 2, color);
    if(pixels & 0x08) al_put_pixel(x, y - 3, color);
    if(pixels & 0x10) al_put_pixel(x, y - 4, color);
    if(pixels & 0x20) al_put_pixel(x, y - 5, color);
    if(pixels & 0x40) al_put_pixel(x, y - 6, color);
    if(pixels & 0x80) al_put_pixel(x, y - 7, color);
  }
}

int main(int argc, char** argv) {
  Invaders si = {0};
  m8080 c = {0};
  c.userdata = &si;

  invaders_init(&si);
  invaders_al_init();

  uint16_t next_interrupt = M8080_RST_1;
  al_start_timer(timer);
  while(1) {
    ALLEGRO_EVENT ev;
    al_wait_for_event(event_queue, &ev);
    if(ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) break;

    invaders_handle_keyboard(&si, ev);

    // space invaders expects two screen interrupts every frame, RST 1 when the
    // screen is near the middle of the current frame and RST 2 when the screen
    // finishes drawing it
    if(ev.type == ALLEGRO_EVENT_TIMER) {
      while(c.cycles < 2000000 / 120) m8080_step(&c);
      c.cycles -= 2000000 / 120;

      m8080_interrupt(&c, next_interrupt);
      if(next_interrupt == M8080_RST_1) {
        next_interrupt = M8080_RST_2;
      } else {
        // draw the screen at once on end-of-screen interrupt
        next_interrupt = M8080_RST_1;
        invaders_draw(&c, bitmap);
        al_set_target_backbuffer(display);
        al_draw_bitmap(bitmap, 0.f, 0.f, 0);
        al_flip_display();
      }
    }
  }

  al_destroy_bitmap(bitmap);
  al_destroy_event_queue(event_queue);
  al_destroy_timer(timer);
  al_destroy_display(display);

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
