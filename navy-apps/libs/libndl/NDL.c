#include <NDL.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static int evtdev = -1;
static int fbdev = -1;
static int dispinfo = -1;
static int sbdev = -1;
static int sbctl = -1;

static int screen_w = 0, screen_h = 0;
static int canvas_w = 0, canvas_h = 0;
static int canvas_x = 0, canvas_y = 0;

uint32_t NDL_GetTicks() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

int NDL_PollEvent(char *buf, int len) {
  if (evtdev < 0) return 0;
  int n = read(evtdev, buf, len);
  if (n <= 0) return 0;

  if (n < len) buf[n] = '\0';
  static int ctrl_down = 0;
  char type[8] = {};
  char key[32] = {};
  if (sscanf(buf, "%7s %31s", type, key) == 2) {
    int is_down = strcmp(type, "kd") == 0;
    if (strcmp(key, "LCTRL") == 0 || strcmp(key, "RCTRL") == 0) {
      ctrl_down = is_down;
    } else if (is_down && ctrl_down && strcmp(key, "C") == 0) {
      exit(130);
    }
  }

  return n;
}

void NDL_OpenCanvas(int *w, int *h) {
  assert(dispinfo >= 0);

  char buf[64];
  lseek(dispinfo, 0, SEEK_SET);
  int n = read(dispinfo, buf, sizeof(buf) - 1);
  assert(n > 0);
  buf[n] = '\0';

  sscanf(buf, "WIDTH:%d\nHEIGHT:%d\n", &screen_w, &screen_h);

  if (w == NULL || h == NULL || *w == 0 || *h == 0) {
    canvas_w = screen_w;
    canvas_h = screen_h;
    if (w) *w = canvas_w;
    if (h) *h = canvas_h;
  } else {
    canvas_w = screen_w;
    canvas_h = screen_h;
    *w = canvas_w;
    *h = canvas_h;
  }

  canvas_x = (screen_w - canvas_w) / 2;
  canvas_y = (screen_h - canvas_h) / 2;
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  assert(fbdev >= 0);
  assert(pixels != NULL);

  if (x == 0 && w == screen_w) {
    off_t off = (canvas_y + y) * screen_w * 4;
    lseek(fbdev, off, SEEK_SET);
    write(fbdev, pixels, w * h * 4);
    return;
  }

  for (int j = 0; j < h; j++) {
    off_t off = ((canvas_y + y + j) * screen_w + (canvas_x + x)) * 4;
    lseek(fbdev, off, SEEK_SET);
    write(fbdev, pixels + j * w, w * 4);
  }
}

void NDL_OpenAudio(int freq, int channels, int samples) {
  if (sbdev < 0) {
    sbdev = open("/dev/sb", O_WRONLY, 0);
  }
  if (sbctl < 0) {
    sbctl = open("/dev/sbctl", O_RDWR, 0);
  }

  assert(sbdev >= 0 && sbctl >= 0);

  int cfg[3];
  cfg[0] = freq;
  cfg[1] = channels;
  cfg[2] = samples;

  int ret = write(sbctl, cfg, sizeof(cfg));
  assert(ret == (int)sizeof(cfg));
}

void NDL_CloseAudio() {
  if (sbdev >= 0) {
    close(sbdev);
    sbdev = -1;
  }
  if (sbctl >= 0) {
    close(sbctl);
    sbctl = -1;
  }
}

int NDL_PlayAudio(void *buf, int len) {
  assert(sbdev >= 0);
  if (buf == NULL || len <= 0) return 0;
  return write(sbdev, buf, len);
}

int NDL_QueryAudio() {
  assert(sbctl >= 0);

  int free_bytes = 0;
  int ret = read(sbctl, &free_bytes, sizeof(free_bytes));
  assert(ret == (int)sizeof(free_bytes));
  return free_bytes;
}

int NDL_Init(uint32_t flags) {
  (void)flags;
  evtdev = open("/dev/events", O_RDONLY, 0);
  fbdev = open("/dev/fb", O_RDWR, 0);
  dispinfo = open("/proc/dispinfo", O_RDONLY, 0);

  assert(evtdev >= 0);
  assert(fbdev >= 0);
  assert(dispinfo >= 0);

  return 0;
}

void NDL_Quit() {
  if (evtdev >= 0) close(evtdev);
  if (fbdev >= 0) close(fbdev);
  if (dispinfo >= 0) close(dispinfo);
  NDL_CloseAudio();

  evtdev = fbdev = dispinfo = -1;
}
