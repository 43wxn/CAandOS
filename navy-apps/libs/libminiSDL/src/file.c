#include <sdl-file.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rw_stdio_seek(SDL_RWops *ctx, int offset, int whence) {
  assert(ctx);
  assert(ctx->hidden.stdio.fp);
  fseek(ctx->hidden.stdio.fp, offset, whence);
  return ftell(ctx->hidden.stdio.fp);
}

static int rw_stdio_read(SDL_RWops *ctx, void *ptr, int size, int maxnum) {
  assert(ctx);
  assert(ctx->hidden.stdio.fp);
  return fread(ptr, size, maxnum, ctx->hidden.stdio.fp);
}

static int rw_stdio_write(SDL_RWops *ctx, const void *ptr, int size, int num) {
  assert(ctx);
  assert(ctx->hidden.stdio.fp);
  return fwrite(ptr, size, num, ctx->hidden.stdio.fp);
}

static int rw_stdio_close(SDL_RWops *ctx) {
  assert(ctx);
  int ret = 0;
  if (ctx->hidden.stdio.fp) {
    ret = fclose(ctx->hidden.stdio.fp);
  }
  free(ctx);
  return ret;
}

static int rw_mem_seek(SDL_RWops *ctx, int offset, int whence) {
  assert(ctx);
  uint8_t *base = (uint8_t *)ctx->hidden.mem.base;
  uint8_t *here = (uint8_t *)ctx->hidden.mem.here;
  uint8_t *stop = (uint8_t *)ctx->hidden.mem.stop;
  uint8_t *newpos = NULL;

  switch (whence) {
    case SEEK_SET: newpos = base + offset; break;
    case SEEK_CUR: newpos = here + offset; break;
    case SEEK_END: newpos = stop + offset; break;
    default: assert(0);
  }

  if (newpos < base) newpos = base;
  if (newpos > stop) newpos = stop;

  ctx->hidden.mem.here = newpos;
  return (int)(newpos - base);
}

static int rw_mem_read(SDL_RWops *ctx, void *ptr, int size, int maxnum) {
  assert(ctx);
  if (size <= 0 || maxnum <= 0) return 0;

  uint8_t *here = (uint8_t *)ctx->hidden.mem.here;
  uint8_t *stop = (uint8_t *)ctx->hidden.mem.stop;
  int bytes = size * maxnum;
  int remain = (int)(stop - here);
  if (remain <= 0) return 0;
  if (bytes > remain) bytes = remain;

  memcpy(ptr, here, bytes);
  ctx->hidden.mem.here = here + bytes;
  return bytes / size;
}

static int rw_mem_write(SDL_RWops *ctx, const void *ptr, int size, int num) {
  assert(ctx);
  if (size <= 0 || num <= 0) return 0;

  uint8_t *here = (uint8_t *)ctx->hidden.mem.here;
  uint8_t *stop = (uint8_t *)ctx->hidden.mem.stop;
  int bytes = size * num;
  int remain = (int)(stop - here);
  if (remain <= 0) return 0;
  if (bytes > remain) bytes = remain;

  memcpy(here, ptr, bytes);
  ctx->hidden.mem.here = here + bytes;
  return bytes / size;
}

static int rw_mem_close(SDL_RWops *ctx) {
  free(ctx);
  return 0;
}

SDL_RWops* SDL_RWFromFile(const char *filename, const char *mode) {
  FILE *fp = fopen(filename, mode);
  if (fp == NULL) return NULL;

  SDL_RWops *ops = (SDL_RWops *)malloc(sizeof(SDL_RWops));
  assert(ops);

  ops->seek = rw_stdio_seek;
  ops->read = rw_stdio_read;
  ops->write = rw_stdio_write;
  ops->close = rw_stdio_close;
  ops->hidden.stdio.fp = fp;
  return ops;
}

SDL_RWops* SDL_RWFromMem(void *mem, int size) {
  assert(mem);
  assert(size >= 0);

  SDL_RWops *ops = (SDL_RWops *)malloc(sizeof(SDL_RWops));
  assert(ops);

  ops->seek = rw_mem_seek;
  ops->read = rw_mem_read;
  ops->write = rw_mem_write;
  ops->close = rw_mem_close;
  ops->hidden.mem.base = mem;
  ops->hidden.mem.here = mem;
  ops->hidden.mem.stop = (uint8_t *)mem + size;
  return ops;
}