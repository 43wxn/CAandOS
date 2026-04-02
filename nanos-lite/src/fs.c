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
  FD_STDIN,
  FD_STDOUT,
  FD_STDERR,
  FD_EVENTS,
  FD_DISPINFO,
  FD_FB,
  FD_SB,
  FD_SBCTL
};

static size_t invalid_read(void *buf, size_t offset, size_t len) {
  (void)buf; (void)offset; (void)len;
  panic("invalid_read");
  return 0;
}

static size_t invalid_write(const void *buf, size_t offset, size_t len) {
  (void)buf; (void)offset; (void)len;
  panic("invalid_write");
  return 0;
}

/* ---------------- audio device ---------------- */

static size_t sbctl_read(void *buf, size_t offset, size_t len) {
  (void)offset;
  assert(len >= sizeof(int));

  AM_AUDIO_STATUS_T stat;
  ioe_read(AM_AUDIO_STATUS, &stat);

  memcpy(buf, &stat.count, sizeof(int));
  return sizeof(int);
}

static size_t sbctl_write(const void *buf, size_t offset, size_t len) {
  (void)offset;
  assert(len >= 3 * sizeof(int));

  const int *cfg = (const int *)buf;

  AM_AUDIO_CTRL_T ctl = {
    .freq = cfg[0],
    .channels = cfg[1],
    .samples = cfg[2],
  };
  ioe_write(AM_AUDIO_CTRL, &ctl);

  return 3 * sizeof(int);
}

static size_t sb_write(const void *buf, size_t offset, size_t len) {
  (void)offset;
  if (len == 0) return 0;

  const uint8_t *p = (const uint8_t *)buf;
  size_t written = 0;

  while (written < len) {
    AM_AUDIO_STATUS_T stat;
    ioe_read(AM_AUDIO_STATUS, &stat);

    int free_bytes = stat.count;
    if (free_bytes <= 0) continue;

    size_t n = len - written;
    if (n > (size_t)free_bytes) n = (size_t)free_bytes;

    AM_AUDIO_PLAY_T ctl = {
      .buf = {
        .start = (void *)(p + written),
        .end   = (void *)(p + written + n),
      },
    };
    ioe_write(AM_AUDIO_PLAY, &ctl);

    written += n;
  }

  return written;
}

/* ---------------- file table ---------------- */

static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]    = {"stdin",          0, 0, invalid_read,  invalid_write, 0},
  [FD_STDOUT]   = {"stdout",         0, 0, invalid_read,  serial_write,  0},
  [FD_STDERR]   = {"stderr",         0, 0, invalid_read,  serial_write,  0},
  [FD_EVENTS]   = {"/dev/events",    0, 0, events_read,   invalid_write, 0},
  [FD_DISPINFO] = {"/proc/dispinfo", 0, 0, dispinfo_read, invalid_write, 0},
  [FD_FB]       = {"/dev/fb",        0, 0, invalid_read,  fb_write,      0},
  [FD_SB]       = {"/dev/sb",        0, 0, invalid_read,  sb_write,      0},
  [FD_SBCTL]    = {"/dev/sbctl",     0, 0, sbctl_read,    sbctl_write,   0},
#include "files.h"
};

#define NR_FILES (int)(sizeof(file_table) / sizeof(file_table[0]))

void init_fs() {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  file_table[FD_FB].size = cfg.width * cfg.height * sizeof(uint32_t);
}

int fs_open(const char *pathname, int flags, int mode) {
  (void)flags;
  (void)mode;
  for (int i = 0; i < NR_FILES; i++) {
    if (strcmp(pathname, file_table[i].name) == 0) {
      file_table[i].open_offset = 0;
      return i;
    }
  }
  return -1;
}

size_t fs_read(int fd, void *buf, size_t len) {
  assert(fd >= 0 && fd < NR_FILES);
  Finfo *f = &file_table[fd];

  if (fd == FD_EVENTS) {
    return events_read(buf, 0, len);
  }

  if (f->read != NULL && f->read != invalid_read) {
    size_t ret = f->read(buf, f->open_offset, len);
    f->open_offset += ret;
    return ret;
  }

  if (f->open_offset >= f->size) {
    return 0;
  }

  size_t real_len = len;
  if (f->open_offset + real_len > f->size) {
    real_len = f->size - f->open_offset;
  }

  ramdisk_read(buf, f->disk_offset + f->open_offset, real_len);
  f->open_offset += real_len;
  return real_len;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  assert(fd >= 0 && fd < NR_FILES);
  Finfo *f = &file_table[fd];

  if ((fd == FD_STDOUT || fd == FD_STDERR) &&
      f->write != NULL && f->write != invalid_write) {
    return f->write(buf, 0, len);
  }

  if (f->write != NULL && f->write != invalid_write) {
    size_t ret = f->write(buf, f->open_offset, len);
    f->open_offset += ret;
    return ret;
  }

  if (f->open_offset >= f->size) {
    return 0;
  }

  size_t real_len = len;
  if (f->open_offset + real_len > f->size) {
    real_len = f->size - f->open_offset;
  }

  ramdisk_write(buf, f->disk_offset + f->open_offset, real_len);
  f->open_offset += real_len;
  return real_len;
}

size_t fs_lseek(int fd, off_t offset, int whence) {
  assert(fd >= 0 && fd < NR_FILES);
  Finfo *f = &file_table[fd];

  assert(fd != FD_SB && fd != FD_SBCTL);

  off_t new_offset = 0;
  switch (whence) {
    case SEEK_SET: new_offset = offset; break;
    case SEEK_CUR: new_offset = (off_t)f->open_offset + offset; break;
    case SEEK_END: new_offset = (off_t)f->size + offset; break;
    default: panic("fs_lseek: invalid whence = %d", whence);
  }

  assert(new_offset >= 0);
  assert((size_t)new_offset <= f->size);
  f->open_offset = (size_t)new_offset;
  return f->open_offset;
}

int fs_close(int fd) {
  assert(fd >= 0 && fd < NR_FILES);
  return 0;
}

int fs_fstat(int fd, struct stat *buf) {
  assert(fd >= 0 && fd < NR_FILES);
  assert(buf != NULL);

  if (fd == FD_STDIN || fd == FD_STDOUT || fd == FD_STDERR ||
      fd == FD_EVENTS || fd == FD_DISPINFO || fd == FD_FB ||
      fd == FD_SB || fd == FD_SBCTL) {
    buf->st_mode = S_IFCHR;
    buf->st_size = 0;
  } else {
    buf->st_mode = S_IFREG;
    buf->st_size = file_table[fd].size;
  }

  return 0;
}