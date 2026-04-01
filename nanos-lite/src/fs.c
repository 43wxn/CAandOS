#include <fs.h>
#include <common.h>
#include <sys/stat.h>
#include <string.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

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
  FD_STDIN, FD_STDOUT, FD_STDERR,
  FD_EVENTS, FD_DISPINFO, FD_FB
};

static size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("invalid_read"); return 0;
}
static size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("invalid_write"); return 0;
}

static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]    = {"stdin",          0, 0, invalid_read,  invalid_write, 0},
  [FD_STDOUT]   = {"stdout",         0, 0, invalid_read,  serial_write,  0},
  [FD_STDERR]   = {"stderr",         0, 0, invalid_read,  serial_write,  0},
  [FD_EVENTS]   = {"/dev/events",    0, 0, events_read,   invalid_write, 0},
  [FD_DISPINFO] = {"/proc/dispinfo", 0, 0, dispinfo_read, invalid_write, 0},
  [FD_FB]       = {"/dev/fb",        0, 0, invalid_read,  fb_write,      0},
#include "files.h"
};

#define NR_FILES (int)(sizeof(file_table) / sizeof(file_table[0]))

void init_fs() {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  file_table[FD_FB].size = cfg.width * cfg.height * 4;
}

int fs_open(const char *path, int flags, int mode) {
  for (int i = 0; i < NR_FILES; i++) {
    if (!strcmp(path, file_table[i].name)) {
      file_table[i].open_offset = 0;
      return i;
    }
  }
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len) {
  Finfo *f = &file_table[fd];
  if (f->read && f->read != invalid_read) {
    size_t ret = f->read(buf, f->open_offset, len);
    f->open_offset += ret;
    return ret;
  }
  if (f->open_offset >= f->size) return 0;
  size_t r = len < f->size - f->open_offset ? len : f->size - f->open_offset;
  ramdisk_read(buf, f->disk_offset + f->open_offset, r);
  f->open_offset += r;
  return r;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  Finfo *f = &file_table[fd];
  size_t ret = 0;
  if (f->write && f->write != invalid_write) {
    ret = f->write(buf, f->open_offset, len);
  } else {
    if (f->open_offset >= f->size) return 0;
    size_t w = len;
    if (f->open_offset + w > f->size) w = f->size - f->open_offset;
    ramdisk_write(buf, f->disk_offset + f->open_offset, w);
    ret = w;
  }
  f->open_offset += ret;
  return ret;
}

off_t fs_lseek(int fd, off_t offset, int whence) {
  Finfo *f = &file_table[fd];
  if (fd <= FD_FB) return -1;
  off_t noff = 0;
  switch(whence) {
    case SEEK_SET: noff = offset; break;
    case SEEK_CUR: noff = f->open_offset + offset; break;
    case SEEK_END: noff = f->size + offset; break;
    default: return -1;
  }
  if (noff < 0 || (size_t)noff > f->size) return -1;
  f->open_offset = noff;
  return noff;
}

int fs_close(int fd) { return 0; }

int fs_fstat(int fd, struct stat *st) {
  memset(st, 0, sizeof(*st));
  if (fd <= FD_FB) {
    st->st_mode = S_IFCHR | 0666;
    st->st_size = 0;
  } else {
    st->st_mode = S_IFREG | 0644;
    st->st_size = file_table[fd].size;
  }
  st->st_nlink = 1;
  st->st_blksize = 4096;
  return 0;
}