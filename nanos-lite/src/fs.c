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

enum { FD_STDIN, FD_STDOUT, FD_STDERR, FD_EVENTS, FD_DISPINFO, FD_FB };

static size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("invalid_read from fd"); return 0;
}
static size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("invalid_write to fd"); return 0;
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
  file_table[FD_FB].size = cfg.width * cfg.height * sizeof(uint32_t);
}

int fs_open(const char *pathname, int flags, int mode) {
  for (int i = 0; i < NR_FILES; i++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      return i;
    }
  }
  panic("File not found: %s", pathname);
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len) {
  Finfo *f = &file_table[fd];
  if (f->read) {
    size_t n = f->read(buf, f->open_offset, len);
    f->open_offset += n;
    return n;
  }
  size_t remain = f->size - f->open_offset;
  size_t real_len = (len < remain) ? len : remain;
  ramdisk_read(buf, f->disk_offset + f->open_offset, real_len);
  f->open_offset += real_len;
  return real_len;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  Finfo *f = &file_table[fd];
  if (f->write) {
    size_t n = f->write(buf, f->open_offset, len);
    f->open_offset += n;
    return n;
  }
  size_t remain = f->size - f->open_offset;
  size_t real_len = (len < remain) ? len : remain;
  ramdisk_write(buf, f->disk_offset + f->open_offset, real_len);
  f->open_offset += real_len;
  return real_len;
}

off_t fs_lseek(int fd, off_t offset, int whence) {
  Finfo *f = &file_table[fd];
  if (fd < 6) return 0; // 特殊设备不支持偏移改变

  off_t new_off = f->open_offset;
  switch (whence) {
    case SEEK_SET: new_off = offset; break;
    case SEEK_CUR: new_off += offset; break;
    case SEEK_END: new_off = f->size + offset; break;
    default: return -1;
  }
  if (new_off < 0 || new_off > f->size) return -1;
  f->open_offset = new_off;
  return new_off;
}

int fs_close(int fd) { return 0; }

int fs_fstat(int fd, struct stat *buf) {
  if (fd < 0 || fd >= NR_FILES) return -1;
  buf->st_size = file_table[fd].size;
  buf->st_mode = (fd < 6) ? S_IFCHR : S_IFREG;
  return 0;
}