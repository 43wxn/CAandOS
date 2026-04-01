#include <fs.h>
#include <common.h>
#include <sys/stat.h>
#include <string.h>
#include <am.h>

typedef size_t (*ReadFn)(void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn)(const void *buf, size_t offset, size_t len);

size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);
size_t serial_write(const void *buf, size_t offset, size_t len);
size_t events_read(void *buf, size_t offset, size_t len);
size_t dispinfo_read(void *buf, size_t offset, size_t len);
size_t fb_write(const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
  size_t open_offset;
} Finfo;

enum {
  FD_STDIN, FD_STDOUT, FD_STDERR, FD_EVENTS, FD_DISPINFO, FD_FB
};

static size_t invalid_read(void *buf, size_t offset, size_t len) { return 0; }
static size_t invalid_write(const void *buf, size_t offset, size_t len) { return 0; }

static Finfo file_table[] = {
  [FD_STDIN]    = {"stdin",          0, 0, invalid_read,  invalid_write, 0},
  [FD_STDOUT]   = {"stdout",         0, 0, invalid_read,  serial_write,  0},
  [FD_STDERR]   = {"stderr",         0, 0, invalid_read,  serial_write,  0},
  [FD_EVENTS]   = {"/dev/events",    0, 0, events_read,   invalid_write, 0},
  [FD_DISPINFO] = {"/proc/dispinfo", 0, 0, dispinfo_read, invalid_write, 0},
  [FD_FB]       = {"/dev/fb",        0, 0, invalid_read,  fb_write,      0},
#include "files.h"
};

#define NR_FILES (sizeof(file_table) / sizeof(file_table[0]))

void init_fs() {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  file_table[FD_FB].size = cfg.width * cfg.height * 4;
}

int fs_open(const char *pathname, int flags, int mode) {
  for (int i = 0; i < NR_FILES; i++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      return i;
    }
  }
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len) {
  if (fd < 0 || fd >= NR_FILES) return 0;
  Finfo *f = &file_table[fd];
  size_t ret = f->read(buf, f->open_offset, len);
  f->open_offset += ret;
  return ret;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  if (fd < 0 || fd >= NR_FILES) return 0;
  Finfo *f = &file_table[fd];
  size_t ret = f->write(buf, f->open_offset, len);
  f->open_offset += ret;
  return ret;
}

// 修复：删除无用变量，无警告编译
size_t fs_lseek(int fd, off_t offset, int whence) {
  if (fd < 0 || fd >= NR_FILES) return -1;
  Finfo *f = &file_table[fd];
  off_t new_off;

  switch (whence) {
    case SEEK_SET: new_off = offset; break;
    case SEEK_CUR: new_off = f->open_offset + offset; break;
    case SEEK_END: new_off = f->size + offset; break;
    default: return -1;
  }

  if (new_off < 0 || (size_t)new_off > f->size) return -1;
  f->open_offset = new_off;
  return new_off;
}

int fs_close(int fd) {
  return (fd >= 0 && fd < NR_FILES) ? 0 : -1;
}

int fs_fstat(int fd, struct stat *buf) {
  if (fd < 0 || fd >= NR_FILES || !buf) return -1;
  Finfo *f = &file_table[fd];
  buf->st_size = f->size;
  buf->st_mode = S_IFREG;
  return 0;
}