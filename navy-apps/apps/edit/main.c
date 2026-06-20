/*
 * edit — SDL-based text editor for nanos-lite
 * All I/O happens in the NEMU SDL window.
 *
 * Usage:  run edit [/path/to/file]
 * Keys:   Ctrl+S=save  Esc=save+quit  Arrows=move  Enter=newline  Backspace=delete
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#define CHAR_W  8
#define CHAR_H 16
#define MAX_LINES 200
#define MAX_COLS  80

#include "font.h"

/* ---- framebuffer ---- */
static SDL_Surface *screen;
static int scr_w, scr_h, scr_pitch;
static uint32_t *scr_pixels;

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) { return (r<<16)|(g<<8)|b; }
static void px(int x, int y, uint32_t c) { if((unsigned)x<(unsigned)scr_w && (unsigned)y<(unsigned)scr_h) scr_pixels[x+y*scr_pitch]=c; }

static void draw_char(int x, int y, char ch, uint32_t fg, uint32_t bg) {
  int idx = (unsigned char)ch; if(idx>=128) idx='?';
  for(int r=0;r<CHAR_H;r++){ unsigned char b=font8x16[idx][r];
    for(int c=0;c<CHAR_W;c++) px(x+c,y+r,(b>>(7-c))&1?fg:bg); }
}
static void draw_str(int x, int y, const char *s, uint32_t fg, uint32_t bg) { while(*s&&x<scr_w){ draw_char(x,y,*s++,fg,bg); x+=CHAR_W; } }
static void fill(int x,int y,int w,int h,uint32_t c){ for(int yy=0;yy<h;yy++) for(int xx=0;xx<w;xx++) px(x+xx,y+yy,c); }
static void flush(void){ SDL_UpdateRect(screen,0,0,scr_w,scr_h); }

/* ---- SDLK → ASCII (verified against NEMU SDL key enum) ---- */
static const char sdlk_lower[128] = {
  [14]='`',  [15]='1',[16]='2',[17]='3',[18]='4',[19]='5',[20]='6',[21]='7',[22]='8',[23]='9',[24]='0',
  [25]='-',  [26]='=',
  [28]='\t',
  [29]='q',[30]='w',[31]='e',[32]='r',[33]='t',[34]='y',[35]='u',[36]='i',[37]='o',[38]='p',
  [39]='[',[40]=']',[41]='\\',
  [43]='a',[44]='s',[45]='d',[46]='f',[47]='g',[48]='h',[49]='j',[50]='k',[51]='l',
  [52]=';',[53]='\'',
  [56]='z',[57]='x',[58]='c',[59]='v',[60]='b',[61]='n',[62]='m',
  [63]=',',[64]='.',[65]='/',
  [70]=' ',
};

static const char sdlk_upper[128] = {
  [14]='~',
  [15]='!',[16]='@',[17]='#',[18]='$',[19]='%',[20]='^',[21]='&',[22]='*',[23]='(',[24]=')',
  [25]='_',[26]='+',
  [29]='Q',[30]='W',[31]='E',[32]='R',[33]='T',[34]='Y',[35]='U',[36]='I',[37]='O',[38]='P',
  [39]='{',[40]='}',[41]='|',
  [43]='A',[44]='S',[45]='D',[46]='F',[47]='G',[48]='H',[49]='J',[50]='K',[51]='L',
  [52]=':',[53]='"',
  [56]='Z',[57]='X',[58]='C',[59]='V',[60]='B',[61]='N',[62]='M',
  [63]='<',[64]='>',[65]='?',
};

static int sdlk_to_ascii(int sym, const uint8_t *ks) {
  if (sym < 0 || sym >= 128) return -1;
  int shifted = ks && (ks[SDLK_LSHIFT] || ks[SDLK_RSHIFT]);
  char ch = shifted ? sdlk_upper[sym] : sdlk_lower[sym];
  return ch ? ch : -1;
}

/* ---- editor state ---- */
static char *lines[MAX_LINES];
static int nlines, top_line, cursor_x, cursor_y, max_rows;
static char filename[128] = "/untitled.txt";

/* ---- file type detection (for future C compiler integration) ---- */
typedef enum { FT_TEXT, FT_C_SOURCE } FileType;
static FileType file_type = FT_TEXT;

static FileType detect_file_type(const char *path) {
  if (path == NULL) return FT_TEXT;
  size_t len = strlen(path);
  if (len >= 2 && strcmp(path + len - 2, ".c") == 0)  return FT_C_SOURCE;
  if (len >= 3 && strcmp(path + len - 3, ".h") == 0)  return FT_C_SOURCE;
  return FT_TEXT;
}

static const char *file_type_label(FileType ft) {
  switch (ft) {
    case FT_C_SOURCE: return "C";
    default:          return "text";
  }
}

/*
 * ---- 未来 C 编译器集成钩子 ----
 * 当 FileType == FT_C_SOURCE 时，可以添加 Ctrl+B (build) 快捷键：
 *   1. Ctrl+S 先保存文件
 *   2. 调用 execve("/bin/tcc", ["/bin/tcc", filename, "-o", output_path], envp)
 *   3. 编译成功 → 显示 "[build OK]"，失败 → 显示错误信息
 *   4. 后续可用 run /tmp/a.out 运行编译结果
 *
 * 需要在 navy-apps 中先添加 tcc (Tiny C Compiler) 作为用户程序。
 */

#define BG           rgb(30,30,30)
#define FG           rgb(200,200,200)
#define CURSOR_BG    rgb(60,60,100)
#define STATUSBAR_BG rgb(50,50,180)
#define STATUSBAR_FG rgb(255,255,255)

static void ensure_line(int ln) {
  while (ln >= nlines && nlines < MAX_LINES) {
    lines[nlines] = (char *)calloc(MAX_COLS + 1, 1);
    if (lines[nlines]) nlines++; else break;
  }
}

static void load_file(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return;
  char buf[MAX_COLS + 2];
  int n, pos = 0;
  while ((n = read(fd, buf + pos, 1)) > 0) {
    if (buf[pos] == '\n' || buf[pos] == '\r' || pos >= MAX_COLS) {
      buf[pos] = '\0';
      ensure_line(nlines);
      if (nlines < MAX_LINES && lines[nlines-1]) strncpy(lines[nlines-1], buf, MAX_COLS);
      pos = 0;
    } else pos++;
  }
  if (pos > 0) { buf[pos]='\0'; ensure_line(nlines); if (nlines<MAX_LINES && lines[nlines-1]) strncpy(lines[nlines-1], buf, MAX_COLS); }
  close(fd);
  if (nlines == 0) ensure_line(0);
}

static void show_status(const char *msg, uint32_t bg) {
  fill(0, scr_h - CHAR_H, scr_w, CHAR_H, bg);
  draw_str(0, scr_h - CHAR_H, msg, STATUSBAR_FG, bg);
  flush();
}

static void save_file(void) {
  int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fd < 0) {
    show_status(" [save FAILED: cannot open file] ", rgb(180,30,30));
    return;
  }
  for (int i = 0; i < nlines; i++) {
    if (lines[i] == NULL) continue;
    int n1 = write(fd, lines[i], strlen(lines[i]));
    int n2 = write(fd, "\n", 1);
    if (n1 < 0 || n2 < 0) {
      close(fd);
      show_status(" [save FAILED: write error] ", rgb(180,30,30));
      return;
    }
  }
  close(fd);
  show_status(" [saved] ", rgb(0,120,0));
}

static void insert_ch(char ch) {
  if (cursor_y >= nlines || lines[cursor_y] == NULL) return;
  char *line = lines[cursor_y];
  int len = strlen(line);
  if (len >= MAX_COLS) return;
  memmove(line + cursor_x + 1, line + cursor_x, len - cursor_x + 1);
  line[cursor_x] = ch;
  cursor_x++;
}

static void backspace_ch(void) {
  if (cursor_y >= nlines || lines[cursor_y] == NULL) return;
  if (cursor_x > 0) {
    char *line = lines[cursor_y];
    int len = strlen(line);
    memmove(line + cursor_x - 1, line + cursor_x, len - cursor_x + 1);
    cursor_x--;
  } else if (cursor_y > 0 && lines[cursor_y-1]) {
    char *prev = lines[cursor_y-1], *cur = lines[cursor_y];
    int pl = strlen(prev), cl = strlen(cur);
    if (pl + cl < MAX_COLS) {
      strcpy(prev + pl, cur);
      free(cur);
      for (int i = cursor_y; i < nlines - 1; i++) lines[i] = lines[i + 1];
      nlines--;
      cursor_y--;
      cursor_x = pl;
    }
  }
}

static void newline_ch(void) {
  ensure_line(nlines + 1);
  if (nlines >= MAX_LINES) return;
  char *cur = lines[cursor_y];
  int len = strlen(cur);
  char *rest = (char *)calloc(MAX_COLS + 1, 1);
  if (rest == NULL) return;
  if (cursor_x < len) strncpy(rest, cur + cursor_x, len - cursor_x);
  cur[cursor_x] = '\0';
  for (int i = nlines - 1; i > cursor_y; i--) lines[i] = lines[i - 1];
  lines[cursor_y + 1] = rest;
  nlines++;
  cursor_y++;
  cursor_x = 0;
}

static void clamp_cursor(void) {
  if (cursor_y >= nlines) cursor_y = nlines - 1;
  if (cursor_y < 0) cursor_y = 0;
  if (lines[cursor_y] == NULL) return;
  int len = strlen(lines[cursor_y]);
  if (cursor_x > len) cursor_x = len;
  if (cursor_x < 0) cursor_x = 0;
}

static void render(void) {
  fill(0, 0, scr_w, scr_h - CHAR_H, BG);
  int rows = (scr_h - CHAR_H) / CHAR_H;
  for (int i = 0; i < rows && top_line + i < nlines; i++) {
    int ln = top_line + i;
    if (lines[ln] == NULL) continue;
    draw_str(0, i * CHAR_H, lines[ln], FG, BG);
    int len = strlen(lines[ln]);
    if (len * CHAR_W < scr_w) fill(len * CHAR_W, i * CHAR_H, scr_w - len * CHAR_W, CHAR_H, BG);
  }
  int cy = (cursor_y - top_line) * CHAR_H;
  if (cy >= 0 && cy < scr_h - CHAR_H) fill(cursor_x * CHAR_W, cy, CHAR_W, CHAR_H, CURSOR_BG);
  fill(0, scr_h - CHAR_H, scr_w, CHAR_H, STATUSBAR_BG);
  char st[128];
  snprintf(st,sizeof(st)," [%s] %s  Ln %d,%d  Ctrl+S=save  Esc=quit",
      file_type_label(file_type), filename, cursor_y+1, cursor_x+1);
  draw_str(0, scr_h - CHAR_H, st, STATUSBAR_FG, STATUSBAR_BG);
  flush();
}

/* ---- main ---- */
int main(int argc, char *argv[]) {
  if (argc >= 2) {
    strncpy(filename, argv[1], sizeof(filename)-1);
    filename[sizeof(filename)-1]='\0';
  }
  file_type = detect_file_type(filename);

  SDL_Init(SDL_INIT_VIDEO);
  screen = SDL_SetVideoMode(0, 0, 32, SDL_HWSURFACE);
  if (screen == NULL || screen->pixels == NULL || screen->w == 0 || screen->h == 0) return 1;
  scr_w = screen->w; scr_h = screen->h; scr_pitch = screen->w;
  scr_pixels = (uint32_t *)screen->pixels;
  max_rows = (scr_h - CHAR_H) / CHAR_H;

  load_file(filename);
  if (nlines == 0) ensure_line(0);
  render();

  while (1) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type != SDL_KEYDOWN) continue;
      int sym = ev.key.keysym.sym;
      const uint8_t *ks = SDL_GetKeyState(NULL);

      /* Ctrl+S = save */
      if (sym == SDLK_s && (ks[SDLK_LCTRL] || ks[SDLK_RCTRL])) {
        save_file();
        continue;
      }

      switch (sym) {
        case SDLK_ESCAPE:  goto quit;
        case SDLK_RETURN:  newline_ch(); break;
        case SDLK_BACKSPACE: backspace_ch(); break;
        case SDLK_TAB:     insert_ch('\t'); break;
        case SDLK_UP:      if (cursor_y > 0) cursor_y--; clamp_cursor(); break;
        case SDLK_DOWN:    if (cursor_y < nlines-1) cursor_y++; clamp_cursor(); break;
        case SDLK_LEFT:    if (cursor_x > 0) cursor_x--;
                           else if (cursor_y > 0) { cursor_y--; if(lines[cursor_y]) cursor_x=strlen(lines[cursor_y]); }
                           break;
        case SDLK_RIGHT:   if(lines[cursor_y] && cursor_x < (int)strlen(lines[cursor_y])) cursor_x++;
                           else if (cursor_y < nlines-1) { cursor_y++; cursor_x=0; }
                           break;
        case SDLK_PAGEUP:  top_line -= max_rows; if (top_line<0) top_line=0;
                           cursor_y -= max_rows; if (cursor_y<0) cursor_y=0; clamp_cursor(); break;
        case SDLK_PAGEDOWN:top_line += max_rows; if (top_line+max_rows>nlines) top_line=nlines-max_rows;
                           if (top_line<0) top_line=0; cursor_y += max_rows;
                           if (cursor_y>=nlines) cursor_y=nlines-1; clamp_cursor(); break;
        default: {
          int ch = sdlk_to_ascii(sym, ks);
          if (ch >= 32 && ch < 127) insert_ch((char)ch);
          break;
        }
      }

      clamp_cursor();
      if (cursor_y < top_line) top_line = cursor_y;
      if (cursor_y >= top_line + max_rows) top_line = cursor_y - max_rows + 1;
      if (top_line < 0) top_line = 0;
      render();
    }
  }

quit:
  save_file();
  SDL_Quit();
  return 0;
}
