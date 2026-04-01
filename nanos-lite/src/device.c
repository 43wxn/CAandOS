#include <common.h>
#include <stdio.h>
#include <string.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) [AM_KEY_##key] = #key,
static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  const char *p = (const char *)buf;
  for (size_t i = 0; i < len; i++) {
    putch(p[i]);
  }
  return len;
}

// 修复：完整读取事件
size_t events_read(void *buf, size_t offset, size_t len) {
  if (offset != 0 || len == 0) return 0;
  AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
  if (ev.keycode == AM_KEY_NONE) return 0;

  int n = snprintf((char *)buf, len, "%s %s\n",
      ev.keydown ? "kd" : "ku",
      keyname[ev.keycode] ? keyname[ev.keycode] : "UNKNOWN");
  return (n < 0 || (size_t)n > len) ? 0 : (size_t)n;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  char info[64];
  int n = snprintf(info, sizeof(info), "WIDTH:%d\nHEIGHT:%d\n", cfg.width, cfg.height);
  size_t real_len = (size_t)n < len ? (size_t)n : len;
  memcpy(buf, info, real_len);
  return real_len;
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  int w = cfg.width;
  int h = cfg.height;
  const uint32_t *pix = (const uint32_t *)buf;
  size_t cnt = len / 4;
  size_t off = offset / 4;
  int x = off % w;
  int y = off / w;

  while (cnt > 0 && y < h) {
    int n = w - x;
    if ((size_t)n > cnt) n = cnt;
    io_write(AM_GPU_FBDRAW, x, y, (void *)pix, n, 1, true);
    pix += n;
    cnt -= n;
    x = 0; y++;
  }
  return len;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}