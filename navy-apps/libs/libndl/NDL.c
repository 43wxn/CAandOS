#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <assert.h>

static int evtdev = -1;
static int fbdev = -1;
static int dispdev = -1;

static int screen_w = 0, screen_h = 0;
static int canvas_w = 0, canvas_h = 0;
static int canvas_x = 0, canvas_y = 0;

uint32_t NDL_GetTicks() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000u + tv.tv_usec / 1000;
}

int NDL_PollEvent(char *buf, int len) {
  if (evtdev < 0) return 0;
  int n = read(evtdev, buf, len);
  if (n < 0) return 0;
  return n;
}

void NDL_OpenCanvas(int *w, int *h) {
  if (dispdev < 0) {
    dispdev = open("/proc/dispinfo", O_RDONLY, 0);
    assert(dispdev >= 0);
  }
  if (fbdev < 0) {
    fbdev = open("/dev/fb", O_WRONLY, 0);
    assert(fbdev >= 0);
  }

  lseek(dispdev, 0, SEEK_SET);

  char buf[128] = {};
  int n = read(dispdev, buf, sizeof(buf) - 1);
  assert(n > 0);
  buf[n] = '\0';

  sscanf(buf, "WIDTH:%d\nHEIGHT:%d\n", &screen_w, &screen_h);

  if (*w == 0 || *h == 0) {
    canvas_w = screen_w;
    canvas_h = screen_h;
    *w = canvas_w;
    *h = canvas_h;
  } else {
    assert(*w <= screen_w && *h <= screen_h);
    canvas_w = *w;
    canvas_h = *h;
  }

  canvas_x = (screen_w - canvas_w) / 2;
  canvas_y = (screen_h - canvas_h) / 2;
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  assert(fbdev >= 0);
  for (int j = 0; j < h; j++) {
    off_t off = ((canvas_y + y + j) * screen_w + (canvas_x + x)) * 4;
    lseek(fbdev, off, SEEK_SET);
    write(fbdev, pixels + j * w, w * 4);
  }
}

void NDL_OpenAudio(int freq, int channels, int samples) {
  (void)freq;
  (void)channels;
  (void)samples;
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  (void)buf;
  (void)len;
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}

int NDL_Init(uint32_t flags) {
  (void)flags;
  evtdev = open("/dev/events", O_RDONLY, 0);
  return 0;
}

void NDL_Quit() {
  if (evtdev >= 0) close(evtdev);
  if (dispdev >= 0) close(dispdev);
  if (fbdev >= 0) close(fbdev);
  evtdev = dispdev = fbdev = -1;
}