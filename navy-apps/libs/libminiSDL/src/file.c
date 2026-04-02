#include <sdl-file.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
  SDL_RWops rw;
  FILE *fp;
} RWopsStdio;

typedef struct {
  SDL_RWops rw;
  uint8_t *base;
  uint8_t *here;
  uint8_t *stop;
} RWopsMem;

static int64_t rw_stdio_seek(SDL_RWops *ctx, int64_t offset, int whence) {
  assert(ctx);
  RWopsStdio *s = (RWopsStdio *)ctx;
  assert(s->fp);

  if (fseek(s->fp, (long)offset, whence) != 0) {
    return -1;
  }
  return (int64_t)ftell(s->fp);
}

static size_t rw_stdio_read(SDL_RWops *ctx, void *ptr, size_t size, size_t maxnum) {
  assert(ctx);
  RWopsStdio *s = (RWopsStdio *)ctx;
  assert(s->fp);
  return fread(ptr, size, maxnum, s->fp);
}

static size_t rw_stdio_write(SDL_RWops *ctx, const void *ptr, size_t size, size_t num) {
  assert(ctx);
  RWopsStdio *s = (RWopsStdio *)ctx;
  assert(s->fp);
  return fwrite(ptr, size, num, s->fp);
}

static int rw_stdio_close(SDL_RWops *ctx) {
  assert(ctx);
  RWopsStdio *s = (RWopsStdio *)ctx;
  int ret = 0;
  if (s->fp) {
    ret = fclose(s->fp);
  }
  free(s);
  return ret;
}

static int64_t rw_mem_seek(SDL_RWops *ctx, int64_t offset, int whence) {
  assert(ctx);
  RWopsMem *m = (RWopsMem *)ctx;

  uint8_t *newpos = NULL;
  switch (whence) {
    case SEEK_SET: newpos = m->base + offset; break;
    case SEEK_CUR: newpos = m->here + offset; break;
    case SEEK_END: newpos = m->stop + offset; break;
    default: assert(0);
  }

  if (newpos < m->base) newpos = m->base;
  if (newpos > m->stop) newpos = m->stop;

  m->here = newpos;
  return (int64_t)(m->here - m->base);
}

static size_t rw_mem_read(SDL_RWops *ctx, void *ptr, size_t size, size_t maxnum) {
  assert(ctx);
  RWopsMem *m = (RWopsMem *)ctx;

  if (size == 0 || maxnum == 0) return 0;

  size_t bytes = size * maxnum;
  size_t remain = (size_t)(m->stop - m->here);
  if (bytes > remain) bytes = remain;

  memcpy(ptr, m->here, bytes);
  m->here += bytes;
  return bytes / size;
}

static size_t rw_mem_write(SDL_RWops *ctx, const void *ptr, size_t size, size_t num) {
  assert(ctx);
  RWopsMem *m = (RWopsMem *)ctx;

  if (size == 0 || num == 0) return 0;

  size_t bytes = size * num;
  size_t remain = (size_t)(m->stop - m->here);
  if (bytes > remain) bytes = remain;

  memcpy(m->here, ptr, bytes);
  m->here += bytes;
  return bytes / size;
}

static int rw_mem_close(SDL_RWops *ctx) {
  assert(ctx);
  free(ctx);
  return 0;
}

SDL_RWops* SDL_RWFromFile(const char *filename, const char *mode) {
  FILE *fp = fopen(filename, mode);
  if (fp == NULL) return NULL;

  RWopsStdio *ops = (RWopsStdio *)malloc(sizeof(RWopsStdio));
  assert(ops);

  memset(ops, 0, sizeof(*ops));
  ops->rw.seek = rw_stdio_seek;
  ops->rw.read = rw_stdio_read;
  ops->rw.write = rw_stdio_write;
  ops->rw.close = rw_stdio_close;
  ops->fp = fp;

  return (SDL_RWops *)ops;
}

SDL_RWops* SDL_RWFromMem(void *mem, int size) {
  assert(mem);
  assert(size >= 0);

  RWopsMem *ops = (RWopsMem *)malloc(sizeof(RWopsMem));
  assert(ops);

  memset(ops, 0, sizeof(*ops));
  ops->rw.seek = rw_mem_seek;
  ops->rw.read = rw_mem_read;
  ops->rw.write = rw_mem_write;
  ops->rw.close = rw_mem_close;
  ops->base = (uint8_t *)mem;
  ops->here = (uint8_t *)mem;
  ops->stop = (uint8_t *)mem + size;

  return (SDL_RWops *)ops;
}