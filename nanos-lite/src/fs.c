#include <fs.h>
#include <common.h>
#include <sys/stat.h>
#include <string.h>
#include <am.h>
#include <memory.h>

#define FS_O_APPEND 0x0008
#define FS_O_CREAT  0x0200
#define FS_O_TRUNC  0x0400
#define RAMFS_MAX_FILES 32
#define RAMFS_MAX_FILE_SIZE (64 * 1024)
#define RAMFS_NAME_LEN 96

typedef size_t (*ReadFn)(void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn)(const void *buf, size_t offset, size_t len);

size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);
size_t serial_write(const void *buf, size_t offset, size_t len);
size_t events_read(void *buf, size_t offset, size_t len);
size_t dispinfo_read(void *buf, size_t offset, size_t len);
size_t fb_write(const void *buf, size_t offset, size_t len);
static size_t proc_meminfo_read(void *buf, size_t offset, size_t len);
static size_t proc_files_read(void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
  size_t open_offset;
  uint8_t *data;
  size_t capacity;
  bool dynamic;
} Finfo;

enum {
  FD_STDIN,
  FD_STDOUT,
  FD_STDERR,
  FD_EVENTS,
  FD_DISPINFO,
  FD_FB,
  FD_SB,
  FD_SBCTL,
  FD_MEMINFO,
  FD_FILES
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

  AM_AUDIO_CONFIG_T cfg;
  AM_AUDIO_STATUS_T stat;
  ioe_read(AM_AUDIO_CONFIG, &cfg);
  ioe_read(AM_AUDIO_STATUS, &stat);

  int free_bytes = cfg.bufsize - stat.count;
  if (free_bytes < 0) free_bytes = 0;

  static int cnt = 0;
  if (cnt < 20) {
    printf("[sbctl_read] bufsize=%d used=%d free=%d\n",
        cfg.bufsize, stat.count, free_bytes);
    cnt++;
  }

  memcpy(buf, &free_bytes, sizeof(int));
  return sizeof(int);
}

static size_t sbctl_write(const void *buf, size_t offset, size_t len) {
  (void)offset;
  assert(len >= 3 * sizeof(int));

  const int *cfg = (const int *)buf;

  printf("[sbctl_write] freq=%d channels=%d samples=%d\n",
      cfg[0], cfg[1], cfg[2]);

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

  static int cnt = 0;

  while (written < len) {
    AM_AUDIO_CONFIG_T cfg;
    AM_AUDIO_STATUS_T stat;
    ioe_read(AM_AUDIO_CONFIG, &cfg);
    ioe_read(AM_AUDIO_STATUS, &stat);

    int free_bytes = cfg.bufsize - stat.count;
    if (free_bytes <= 0) continue;

    size_t n = len - written;
    if (n > (size_t)free_bytes) n = (size_t)free_bytes;

    if (cnt < 20) {
      printf("[sb_write] bufsize=%d used=%d free=%d write=%zu\n",
          cfg.bufsize, stat.count, free_bytes, n);
      cnt++;
    }

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
  [FD_MEMINFO]  = {"/proc/meminfo",  0, 0, proc_meminfo_read, invalid_write, 0},
  [FD_FILES]    = {"/proc/files",    0, 0, proc_files_read,   invalid_write, 0},
#include "files.h"
};

#define NR_FILES (int)(sizeof(file_table) / sizeof(file_table[0]))

static Finfo ramfs[RAMFS_MAX_FILES];
static char ramfs_names[RAMFS_MAX_FILES][RAMFS_NAME_LEN];

static size_t slice_read(const char *src, size_t total, void *buf,
    size_t offset, size_t len) {
  if (offset >= total) return 0;
  size_t real_len = total - offset;
  if (real_len > len) real_len = len;
  memcpy(buf, src + offset, real_len);
  return real_len;
}

static Finfo *get_file(int fd) {
  if (fd >= 0 && fd < NR_FILES) return &file_table[fd];
  fd -= NR_FILES;
  if (fd >= 0 && fd < RAMFS_MAX_FILES && ramfs[fd].name != NULL) {
    return &ramfs[fd];
  }
  return NULL;
}

static int find_file(const char *pathname) {
  for (int i = 0; i < NR_FILES; i++) {
    if (strcmp(pathname, file_table[i].name) == 0) return i;
  }

  for (int i = 0; i < RAMFS_MAX_FILES; i++) {
    if (ramfs[i].name != NULL && strcmp(pathname, ramfs[i].name) == 0) {
      return NR_FILES + i;
    }
  }

  return -1;
}

static int alloc_ramfs_file(const char *pathname) {
  size_t len = strlen(pathname);
  if (len == 0 || len >= RAMFS_NAME_LEN) return -1;

  for (int i = 0; i < RAMFS_MAX_FILES; i++) {
    if (ramfs[i].name == NULL) {
      strcpy(ramfs_names[i], pathname);
      ramfs[i] = (Finfo) {
        .name = ramfs_names[i],
        .size = 0,
        .disk_offset = 0,
        .read = NULL,
        .write = NULL,
        .open_offset = 0,
        .data = NULL,
        .capacity = 0,
        .dynamic = true,
      };
      return NR_FILES + i;
    }
  }

  return -1;
}

static bool ramfs_grow(Finfo *f, size_t need) {
  if (need <= f->capacity) return true;
  if (need > RAMFS_MAX_FILE_SIZE) return false;

  size_t new_cap = ROUNDUP(need, PGSIZE);
  if (new_cap > RAMFS_MAX_FILE_SIZE) new_cap = RAMFS_MAX_FILE_SIZE;

  uint8_t *new_data = new_page(new_cap / PGSIZE);
  if (new_data == NULL) return false;

  if (f->data != NULL && f->size > 0) {
    memcpy(new_data, f->data, f->size);
  }

  f->data = new_data;
  f->capacity = new_cap;
  return true;
}

static size_t ramfs_used_capacity(void) {
  size_t used = 0;
  for (int i = 0; i < RAMFS_MAX_FILES; i++) {
    if (ramfs[i].name != NULL) used += ramfs[i].capacity;
  }
  return used;
}

static size_t proc_meminfo_read(void *buf, size_t offset, size_t len) {
  size_t total = 0, used = 0, free = 0;
  get_memory_info(&total, &used, &free);

  char info[256];
  int n = snprintf(info, sizeof(info),
      "MemTotal: %u bytes\n"
      "MemUsed:  %u bytes\n"
      "MemFree:  %u bytes\n"
      "PageSize: %u bytes\n"
      "RamfsCap: %u bytes\n",
      (unsigned)total, (unsigned)used, (unsigned)free,
      (unsigned)PGSIZE, (unsigned)ramfs_used_capacity());

  return slice_read(info, (size_t)n, buf, offset, len);
}

static size_t proc_files_read(void *buf, size_t offset, size_t len) {
  char info[2048];
  size_t pos = 0;

  pos += snprintf(info + pos, sizeof(info) - pos, "type     size    path\n");
  for (int i = 0; i < NR_FILES && pos < sizeof(info); i++) {
    const char *type = "file";
    if (i == FD_STDIN || i == FD_STDOUT || i == FD_STDERR ||
        i == FD_EVENTS || i == FD_FB || i == FD_SB || i == FD_SBCTL) {
      type = "dev";
    } else if (i == FD_DISPINFO || i == FD_MEMINFO || i == FD_FILES) {
      type = "proc";
    }
    pos += snprintf(info + pos, sizeof(info) - pos, "%-7s %7u %s\n",
        type, (unsigned)file_table[i].size, file_table[i].name);
  }

  for (int i = 0; i < RAMFS_MAX_FILES && pos < sizeof(info); i++) {
    if (ramfs[i].name != NULL) {
      pos += snprintf(info + pos, sizeof(info) - pos, "%-7s %7u %s\n",
          "ramfs", (unsigned)ramfs[i].size, ramfs[i].name);
    }
  }

  if (pos > sizeof(info)) pos = sizeof(info);
  return slice_read(info, pos, buf, offset, len);
}

void init_fs() {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  file_table[FD_FB].size = cfg.width * cfg.height * sizeof(uint32_t);
}

int fs_open(const char *pathname, int flags, int mode) {
  (void)mode;
  int fd = find_file(pathname);
  if (fd < 0 && (flags & FS_O_CREAT)) {
    fd = alloc_ramfs_file(pathname);
  }

  Finfo *f = get_file(fd);
  if (f == NULL) return -1;

  if (f->dynamic && (flags & FS_O_TRUNC)) {
    f->size = 0;
  }
  f->open_offset = (flags & FS_O_APPEND) ? f->size : 0;
  return fd;
}

size_t fs_read(int fd, void *buf, size_t len) {
  Finfo *f = get_file(fd);
  if (f == NULL) return (size_t)-1;

  if (fd == FD_EVENTS) {
    return events_read(buf, 0, len);
  }

  if (f->read != NULL && f->read != invalid_read) {
    size_t ret = f->read(buf, f->open_offset, len);
    f->open_offset += ret;
    return ret;
  }

  if (f->dynamic) {
    if (f->open_offset >= f->size) return 0;

    size_t real_len = len;
    if (f->open_offset + real_len > f->size) {
      real_len = f->size - f->open_offset;
    }

    memcpy(buf, f->data + f->open_offset, real_len);
    f->open_offset += real_len;
    return real_len;
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
  Finfo *f = get_file(fd);
  if (f == NULL) return (size_t)-1;

  if ((fd == FD_STDOUT || fd == FD_STDERR) &&
      f->write != NULL && f->write != invalid_write) {
    return f->write(buf, 0, len);
  }

  if (f->write != NULL && f->write != invalid_write) {
    size_t ret = f->write(buf, f->open_offset, len);
    f->open_offset += ret;
    return ret;
  }

  if (f->dynamic) {
    size_t end = f->open_offset + len;
    if (!ramfs_grow(f, end)) return (size_t)-1;

    memcpy(f->data + f->open_offset, buf, len);
    f->open_offset = end;
    if (end > f->size) f->size = end;
    return len;
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
  Finfo *f = get_file(fd);
  if (f == NULL) return (size_t)-1;

  assert(fd != FD_SB && fd != FD_SBCTL);

  off_t new_offset = 0;
  switch (whence) {
    case SEEK_SET: new_offset = offset; break;
    case SEEK_CUR: new_offset = (off_t)f->open_offset + offset; break;
    case SEEK_END: new_offset = (off_t)f->size + offset; break;
    default: panic("fs_lseek: invalid whence = %d", whence);
  }

  if (new_offset < 0) return (size_t)-1;
  if (!f->dynamic && (size_t)new_offset > f->size) return (size_t)-1;
  if (f->dynamic && (size_t)new_offset > RAMFS_MAX_FILE_SIZE) return (size_t)-1;

  f->open_offset = (size_t)new_offset;
  return f->open_offset;
}

int fs_close(int fd) {
  return get_file(fd) == NULL ? -1 : 0;
}

int fs_fstat(int fd, struct stat *buf) {
  Finfo *f = get_file(fd);
  if (f == NULL || buf == NULL) return -1;

  if (fd == FD_STDIN || fd == FD_STDOUT || fd == FD_STDERR ||
      fd == FD_EVENTS || fd == FD_FB ||
      fd == FD_SB || fd == FD_SBCTL) {
    buf->st_mode = S_IFCHR;
    buf->st_size = 0;
  } else {
    buf->st_mode = S_IFREG;
    buf->st_size = f->size;
  }

  return 0;
}

int fs_unlink(const char *pathname) {
  int fd = find_file(pathname);
  if (fd < NR_FILES) return -1;

  Finfo *f = get_file(fd);
  if (f == NULL || !f->dynamic) return -1;

  int idx = fd - NR_FILES;
  ramfs_names[idx][0] = '\0';
  ramfs[idx] = (Finfo) {0};
  return 0;
}
