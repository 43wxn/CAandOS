#define SDL_malloc  malloc
#define SDL_free    free
#define SDL_realloc realloc

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"

#include <SDL.h>
#include <SDL_image.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *rw_read_all(SDL_RWops *src, int *out_size) {
  assert(src);
  assert(src->seek);
  assert(src->read);

  int64_t cur = src->seek(src, 0, SEEK_CUR);
  if (cur < 0) cur = 0;

  int64_t end = src->seek(src, 0, SEEK_END);
  assert(end >= 0);

  int64_t size = end;
  assert(src->seek(src, 0, SEEK_SET) == 0);

  uint8_t *buf = (uint8_t *)malloc((size_t)size);
  assert(buf);

  size_t got = src->read(src, buf, 1, (size_t)size);
  assert((int64_t)got == size);

  if (out_size) *out_size = (int)size;
  return buf;
}

SDL_Surface* IMG_Load_RW(SDL_RWops *src, int freesrc) {
  if (src == NULL) return NULL;

  int len = 0;
  uint8_t *filebuf = rw_read_all(src, &len);
  if (filebuf == NULL) {
    if (freesrc && src->close) src->close(src);
    return NULL;
  }

  int w, h, n;
  uint8_t *pixels = stbi_load_from_memory(filebuf, len, &w, &h, &n, 4);
  free(filebuf);

  if (freesrc && src->close) {
    src->close(src);
  }

  if (pixels == NULL) {
    return NULL;
  }

  SDL_Surface *s = SDL_CreateRGBSurface(
    0, w, h, 32,
    DEFAULT_RMASK, DEFAULT_GMASK, DEFAULT_BMASK, DEFAULT_AMASK
  );
  assert(s);

  memcpy(s->pixels, pixels, (size_t)w * (size_t)h * 4);
  stbi_image_free(pixels);

  return s;
}

SDL_Surface* IMG_Load(const char *filename) {
  SDL_RWops *src = SDL_RWFromFile(filename, "rb");
  if (src == NULL) return NULL;
  return IMG_Load_RW(src, 1);
}

int IMG_isPNG(SDL_RWops *src) {
  if (src == NULL || src->seek == NULL || src->read == NULL) return 0;

  static const uint8_t png_sig[8] = {
    0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
  };

  int64_t old = src->seek(src, 0, SEEK_CUR);
  if (old < 0) old = 0;

  assert(src->seek(src, 0, SEEK_SET) == 0);

  uint8_t buf[8];
  size_t n = src->read(src, buf, 1, 8);

  src->seek(src, old, SEEK_SET);

  if (n != 8) return 0;
  return memcmp(buf, png_sig, 8) == 0;
}

SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src) {
  return IMG_Load_RW(src, 0);
}

char *IMG_GetError() {
  return "Navy does not support IMG_GetError()";
}