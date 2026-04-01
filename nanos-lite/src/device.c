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
  (void)offset;
  const char *p = (const char *)buf;
  for (size_t i = 0; i < len; i++) {
    putch(p[i]);
  }
  return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  if (len == 0) return 0;

  AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
  if (ev.keycode == AM_KEY_NONE) return 0;

  char tmp[64];
  int n = snprintf(tmp, sizeof(tmp), "%s %s\n",
      ev.keydown ? "kd" : "ku",
      keyname[ev.keycode] ? keyname[ev.keycode] : "UNKNOWN");

  if ((size_t)offset >= (size_t)n) return 0;
  size_t real_len = (size_t)n - offset;
  if (real_len > len) real_len = len;
  memcpy(buf, tmp + offset, real_len);
  return real_len;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);

  char info[64];
  int n = snprintf(info, sizeof(info), "WIDTH:%d\nHEIGHT:%d\n", cfg.width, cfg.height);

  if ((size_t)offset >= (size_t)n) return 0;
  size_t real_len = (size_t)n - offset;
  if (real_len > len) real_len = len;
  memcpy(buf, info + offset, real_len);
  return real_len;
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  int w = cfg.width;
  int h = cfg.height;

  const uint32_t *pixels = (const uint32_t *)buf;
  size_t pixel_off = offset / sizeof(uint32_t);
  size_t pixel_cnt = len / sizeof(uint32_t);

  int x = pixel_off % w;
  int y = pixel_off / w;

  while (pixel_cnt > 0 && y < h) {
    int n = w - x;
    if ((size_t)n > pixel_cnt) n = pixel_cnt;

    io_write(AM_GPU_FBDRAW, x, y, (void *)pixels, n, 1, true);

    pixels += n;
    pixel_cnt -= n;
    x = 0;
    y++;
  }
  return len;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}