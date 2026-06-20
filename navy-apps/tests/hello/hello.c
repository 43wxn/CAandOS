/*
 * hello — SDL-based demo for nanos-lite
 * Renders 5 "Hello World" messages directly on the NEMU SDL window.
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

/* Minimal 8x16 font (subset for "Hello World from Navy-apps 1-5") */
static const unsigned char font[][16] = {
  ['H']={0,198,198,198,254,198,198,198,0,0,0,0,0,0,0,0},
  ['e']={0,0,120,204,252,192,204,120,0,0,0,0,0,0,0,0},
  ['l']={0,56,24,24,24,24,24,60,0,0,0,0,0,0,0,0},
  ['o']={0,0,0,120,204,204,204,120,0,0,0,0,0,0,0,0},
  [' ']={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  ['W']={0,198,198,198,214,214,254,108,0,0,0,0,0,0,0,0},
  ['r']={0,0,0,216,236,192,192,192,0,0,0,0,0,0,0,0},
  ['d']={0,12,12,60,108,204,204,60,0,0,0,0,0,0,0,0},
  ['f']={0,56,108,96,240,96,96,96,0,0,0,0,0,0,0,0},
  ['N']={0,198,230,246,222,206,198,198,0,0,0,0,0,0,0,0},
  ['a']={0,0,120,12,124,204,204,118,0,0,0,0,0,0,0,0},
  ['v']={0,0,0,198,198,198,108,56,0,0,0,0,0,0,0,0},
  ['y']={0,0,0,198,198,108,56,24,96,0,0,0,0,0,0,0},
  ['-']={0,0,0,0,126,0,0,0,0,0,0,0,0,0,0,0},
  ['p']={0,0,0,248,204,204,204,248,192,192,0,0,0,0,0,0},
  ['s']={0,0,0,120,192,120,12,240,0,0,0,0,0,0,0,0},
  ['t']={0,96,96,240,96,96,96,56,0,0,0,0,0,0,0,0},
  ['i']={0,24,0,56,24,24,24,60,0,0,0,0,0,0,0,0},
  ['m']={0,0,0,236,254,214,214,198,0,0,0,0,0,0,0,0},
  ['!']={0,0,24,24,24,24,24,0,24,24,0,0,0,0,0,0},
  ['1']={0,24,56,24,24,24,24,126,0,0,0,0,0,0,0,0},
  ['2']={0,124,198,6,28,112,192,254,0,0,0,0,0,0,0,0},
  ['3']={0,124,198,6,60,6,198,124,0,0,0,0,0,0,0,0},
  ['4']={0,28,60,108,204,254,12,12,0,0,0,0,0,0,0,0},
  ['5']={0,254,192,252,6,6,198,124,0,0,0,0,0,0,0,0},
  ['h']={0,192,192,248,204,204,204,204,0,0,0,0,0,0,0,0},
  [',']={0,0,0,0,0,24,24,48,0,0,0,0,0,0,0,0},
  ['.']={0,0,0,0,0,0,24,24,0,0,0,0,0,0,0,0},
  ['c']={0,0,120,204,192,192,204,120,0,0,0,0,0,0,0,0},
  ['k']={0,192,192,204,216,240,216,204,0,0,0,0,0,0,0,0},
  ['x']={0,0,0,198,108,56,108,198,0,0,0,0,0,0,0,0},
  ['A']={0,56,108,198,198,254,198,198,0,0,0,0,0,0,0,0},
  ['P']={0,252,198,198,252,192,192,192,0,0,0,0,0,0,0,0},
  ['q']={0,0,0,60,108,204,204,60,12,12,0,0,0,0,0,0},
};

static int scr_w = 0, scr_h = 0;
static uint32_t *scr = NULL;

static inline void px(int x, int y, uint32_t c) {
  if (x >= 0 && x < scr_w && y >= 0 && y < scr_h)
    scr[x + y * scr_w] = c;
}

static void draw_char(int x, int y, char ch, uint32_t fg, uint32_t bg) {
  int idx = (unsigned char)ch;
  for (int row = 0; row < 16; row++) {
    unsigned char bits = font[idx][row];
    for (int col = 0; col < 8; col++)
      px(x + col, y + row, (bits >> (7 - col)) & 1 ? fg : bg);
  }
}

static void draw_str(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
  while (*s && x < scr_w) { draw_char(x, y, *s++, fg, bg); x += 8; }
}

int main() {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Surface *s = SDL_SetVideoMode(0, 0, 32, SDL_HWSURFACE);
  scr_w = s->w; scr_h = s->h;
  scr = (uint32_t *)s->pixels;

  uint32_t bg = 0x001e1e1e;
  uint32_t green = 0x004aa02c;
  uint32_t white = 0x00c8c8c8;

  for (int y = 0; y < scr_h; y++)
    for (int x = 0; x < scr_w; x++)
      px(x, y, bg);

  for (int k = 0; k < 5; k++) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Hello World from Navy-apps for the %d%s time!",
        k + 1, k == 0 ? "st" : k == 1 ? "nd" : k == 2 ? "rd" : "th");
    draw_str(16, 16 + k * 20, buf, green, bg);
    SDL_UpdateRect(s, 0, 0, scr_w, scr_h);

    for (volatile int d = 0; d < 8000000; d++) asm volatile("");
  }

  draw_str(16, 16 + 5 * 20, "Press any key to exit...", white, bg);
  SDL_UpdateRect(s, 0, 0, scr_w, scr_h);

  SDL_Event ev;
  while (1) {
    if (SDL_PollEvent(&ev) && ev.type == SDL_KEYDOWN) break;
  }

  SDL_Quit();
  return 0;
}
