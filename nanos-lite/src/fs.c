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
  FD_STDIN,
  FD_STDOUT,
  FD_STDERR,
  FD_EVENTS,
  FD_DISPINFO,
  FD_FB
};

static size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("invalid_read");
  return 0;
}

static size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("invalid_write");
  return 0;
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
  // PA3 file-test 暂时不需要特殊处理 /dev/fb
}

int fs_open(const char *pathname, int flags, int mode) {
  Log("fs_open(pathname=%s, flags=%d, mode=%d)", pathname, flags, mode);
  for (int i = 0; i < NR_FILES; i++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      Log("fs_open matched fd=%d size=%d disk_offset=%d",
          i, (int)file_table[i].size, (int)file_table[i].disk_offset);
      return i;
    }
  }
  panic("fs_open: file not found = %s", pathname);
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len) {
  assert(fd >= 0 && fd < NR_FILES);
  Finfo *f = &file_table[fd];
  Log("fs_read(fd=%d, len=%d, open_offset=%d, size=%d)",
      fd, (int)len, (int)f->open_offset, (int)f->size);

  if (f->read != NULL && f->read != invalid_read) {
    size_t ret = f->read(buf, f->open_offset, len);
    f->open_offset += ret;
    Log("fs_read special -> %d, new_offset=%d", (int)ret, (int)f->open_offset);
    return ret;
  }

  if (f->open_offset >= f->size) {
    Log("fs_read EOF");
    return 0;
  }

  size_t remain = f->size - f->open_offset;
  size_t real_len = len < remain ? len : remain;
  ramdisk_read(buf, f->disk_offset + f->open_offset, real_len);
  f->open_offset += real_len;
  Log("fs_read normal -> %d, new_offset=%d", (int)real_len, (int)f->open_offset);
  return real_len;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  assert(fd >= 0 && fd < NR_FILES);
  Finfo *f = &file_table[fd];

  size_t ret = 0;
  if (f->write != NULL && f->write != invalid_write) {
    ret = f->write(buf, f->open_offset, len);
  } else {
    if (f->open_offset >= f->size) return 0;
    if (f->open_offset + len > f->size) {
      len = f->size - f->open_offset;
    }
    ramdisk_write(buf, f->disk_offset + f->open_offset, len);
    ret = len;
  }

  f->open_offset += ret;
  return ret;
}

size_t fs_lseek(int fd, off_t offset, int whence) {
  assert(fd >= 0 && fd < NR_FILES);
  Finfo *f = &file_table[fd];

  Log("fs_lseek(fd=%d, offset=%d, whence=%d, old=%d, size=%d)",
      fd, (int)offset, whence, (int)f->open_offset, (int)f->size);

  off_t new_offset = 0;
  switch (whence) {
    case SEEK_SET: new_offset = offset; break;
    case SEEK_CUR: new_offset = (off_t)f->open_offset + offset; break;
    case SEEK_END: new_offset = (off_t)f->size + offset; break;
    default: panic("fs_lseek: invalid whence = %d", whence);
  }

  assert(new_offset >= 0 && (size_t)new_offset <= f->size);
  f->open_offset = (size_t)new_offset;
  Log("fs_lseek -> %d", (int)f->open_offset);
  return f->open_offset;
}
int fs_close(int fd) {
  assert(fd >= 0 && fd < NR_FILES);
  return 0;
}

int fs_fstat(int fd, struct stat *buf) {
  assert(fd >= 0 && fd < NR_FILES);
  assert(buf != NULL);

  memset(buf, 0, sizeof(*buf));

  if (fd == FD_STDIN || fd == FD_STDOUT || fd == FD_STDERR ||
      fd == FD_EVENTS || fd == FD_DISPINFO || fd == FD_FB) {
    buf->st_mode = S_IFCHR;
  } else {
    buf->st_mode = S_IFREG;
    buf->st_size = file_table[fd].size;
  }

  buf->st_blksize = 4096;

  Log("fs_fstat(fd=%d) mode=%d size=%d blksize=%d",
      fd, (int)buf->st_mode, (int)buf->st_size, (int)buf->st_blksize);
  return 0;
}