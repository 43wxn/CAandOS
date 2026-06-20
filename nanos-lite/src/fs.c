#include <fs.h>
#include <common.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <am.h>
#include <memory.h>
#include <proc.h>

/*
 * nanos-lite 的文件系统是一个适合 PA/课程设计演示的“小型 VFS”。
 * 它不是把宿主机某个目录实时挂载进来，而是在构建时把 navy-apps/fsimg
 * 打包成 ramdisk，并在运行期通过 file_table[] 描述这些文件。
 *
 * 这里同时支持三类对象：
 *
 *   1. 静态 ramdisk 文件
 *      由 navy-apps/build/ramdisk.img + files.h 生成，开机后可读。
 *      /bin/dterm、/bin/pal、资源文件等都属于这一类。
 *
 *   2. 运行期 ramfs 文件
 *      由 shell 的 touch/write/append 或 open(..., O_CREAT) 创建。
 *      数据存在 new_page() 分配的内存里，关机后丢失。
 *
 *   3. 设备/伪文件
 *      例如 /dev/events、/dev/fb、/proc/meminfo。
 *      它们没有真实磁盘内容，而是通过 read/write 回调连接到 AM 设备
 *      或动态生成文本。
 *
 * syscall.c 只负责把 SYS_open/SYS_read/... 分发到这里；
 * 真正的路径查找、偏移维护、设备回调、读写边界处理都在 fs.c 中完成。
 */

/* 这里的 open flag 数值要和 navy-apps/libos/newlib 使用的 O_* 值保持一致。 */
#define FS_O_APPEND 0x0008
#define FS_O_CREAT  0x0200
#define FS_O_TRUNC  0x0400

/* 运行期 ramfs 的简化限制：最多 128 个文件，每个最多 64 KiB。 */
#define RAMFS_MAX_FILES 128
#define RAMFS_MAX_FILE_SIZE (64 * 1024)
#define RAMFS_NAME_LEN 96
#define PROC_FILES_BUFSIZE (32 * 1024)

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
static size_t proc_processes_read(void *buf, size_t offset, size_t len);

typedef struct {
  char *name;          /* 文件名或设备名，例如 /bin/dterm、/dev/fb。 */
  size_t size;         /* 文件逻辑大小；设备文件可能在 init_fs() 后设置。 */
  size_t disk_offset;  /* 静态 ramdisk 文件在 ramdisk.img 中的起始偏移，用来精确定位文件内容。 */
  ReadFn read;         /* 设备/伪文件的读回调；有它就靠回调精确实现读取。 */
  WriteFn write;       /* 设备文件的写回调；有它就靠回调精确实现写入。 */
  size_t open_offset;  /* 当前读写偏移。和 disk_offset/data 配合定位“文件内第几个字节”。 */
  uint8_t *data;       /* 运行期 ramfs 文件的数据区；每个动态文件有自己的 data 指针。 */
  size_t capacity;     /* ramfs 文件已分配容量，按页增长。 */
  bool dynamic;        /* true 表示这是运行期创建的 ramfs 文件。 */
} Finfo;

/* file_table[] 前几个固定编号同时也是标准 fd/设备 fd。 */
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
  FD_FILES,
  FD_PROCESSES
};

static size_t invalid_read(void *buf, size_t offset, size_t len) {
  (void)buf; (void)offset; (void)len;
  /* 用 panic 暴露“不应该读”的设备，例如 stdout。 */
  panic("invalid_read");
  return 0;
}

static size_t invalid_write(const void *buf, size_t offset, size_t len) {
  (void)buf; (void)offset; (void)len;
  /* 用 panic 暴露“不应该写”的设备，例如 stdin、/proc/meminfo。 */
  panic("invalid_write");
  return 0;
}

/* ---------------- audio device ---------------- */

static size_t sbctl_read(void *buf, size_t offset, size_t len) {
  (void)offset;
  assert(len >= sizeof(int));

  /*
   * /dev/sbctl 的读语义：返回音频缓冲区当前还剩多少字节可写。
   * 用户态 SDL 音频会根据这个值决定能不能继续往 /dev/sb 写声音数据。
   */
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

  /* /dev/sbctl 的写语义：设置 freq/channels/samples 三个音频参数。 */
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

  /*
   * /dev/sb 的写语义：把一段 PCM 数据提交给 AM_AUDIO_PLAY。
   * 这里会忙等直到音频缓冲区有空间；实现简单，但大量音频输出时会拖慢。
   */
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
      printf("[sb_write] bufsize=%d used=%d free=%d write=%u\n",
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

/*
 * 静态文件表是整个 fs.c 的核心。
 *
 * - 前面手写的是标准输入输出、设备文件、/proc 伪文件。
 * - #include "files.h" 会在编译时展开成 ramdisk 中每个普通文件的 Finfo。
 *
 * 所以应用程序 open("/bin/pal") 时，本质是在这个数组中找到对应项；
 * loader 再通过 ramdisk_read() 把 ELF 内容读进内存。
 *
 * Finfo 初始化顺序是：
 *   {name, size, disk_offset, read_callback, write_callback, open_offset}
 * 后面的 data/capacity/dynamic 没写时会默认清零，表示静态文件或设备。
 *
 * fd 不是“文件类型编号”，而是这个表里的一个具体下标：
 *   fd=1 一定对应 stdout；
 *   fd=FD_FB 一定对应 /dev/fb；
 *   files.h 中每个普通文件也都会有自己独立的下标。
 * 因此两个文件就算同为普通文件，也会因为 fd 不同而映射到不同 Finfo。
 */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]    = {"stdin",          0, 0, invalid_read,  invalid_write, 0}, /* fd=0，暂不支持键盘作为 stdin。 */
  [FD_STDOUT]   = {"stdout",         0, 0, invalid_read,  serial_write,  0}, /* fd=1，printf/write 输出到串口/宿主终端。 */
  [FD_STDERR]   = {"stderr",         0, 0, invalid_read,  serial_write,  0}, /* fd=2，错误输出同样走串口。 */
  [FD_EVENTS]   = {"/dev/events",    0, 0, events_read,   invalid_write, 0}, /* 键盘事件设备，NDL/SDL 轮询它。 */
  [FD_DISPINFO] = {"/proc/dispinfo", 0, 0, dispinfo_read, invalid_write, 0}, /* 屏幕宽高信息，NDL_OpenCanvas 会读它。 */
  [FD_FB]       = {"/dev/fb",        0, 0, invalid_read,  fb_write,      0}, /* 帧缓冲设备，图形应用把像素写到这里。 */
  [FD_SB]       = {"/dev/sb",        0, 0, invalid_read,  sb_write,      0}, /* 声音播放数据设备。 */
  [FD_SBCTL]    = {"/dev/sbctl",     0, 0, sbctl_read,    sbctl_write,   0}, /* 声卡控制/状态设备。 */
  [FD_MEMINFO]  = {"/proc/meminfo",  0, 0, proc_meminfo_read, invalid_write, 0}, /* 动态生成内存信息。 */
  [FD_FILES]    = {"/proc/files",    0, 0, proc_files_read,   invalid_write, 0}, /* 动态生成文件表清单。 */
  [FD_PROCESSES] = {"/proc/processes",0, 0, proc_processes_read, invalid_write, 0}, /* 动态生成进程表。 */
  /*
   * files.h 由 navy-apps 构建 ramdisk 时生成，里面会继续追加：
 *   /bin/dterm、/bin/pal、/share/games/pal/...、/home/welcome.txt 等
   * 所有被打进 ramdisk.img 的普通文件。
   */
#include "files.h"
};

#define NR_FILES (int)(sizeof(file_table) / sizeof(file_table[0]))

/*
 * ramfs[] 是运行期新建文件表。为了避免动态分配文件名字符串，
 * 文件名实际存放在 ramfs_names[][]，Finfo.name 指向其中一个槽位。
 */
static Finfo ramfs[RAMFS_MAX_FILES];
static char ramfs_names[RAMFS_MAX_FILES][RAMFS_NAME_LEN];

/* 从一段动态生成的字符串中按 offset/len 截取，用于 /proc 文件读取。 */
static size_t slice_read(const char *src, size_t total, void *buf,
    size_t offset, size_t len) {
  if (offset >= total) return 0;
  size_t real_len = total - offset;
  if (real_len > len) real_len = len;
  memcpy(buf, src + offset, real_len);
  return real_len;
}

/*
 * fd 到 Finfo 的转换：
 *   0 ... NR_FILES-1              -> 静态 file_table[]
 *   NR_FILES ... NR_FILES+RAMFS_MAX_FILES-1 -> 运行期 ramfs[]
 *
 * 可以把 fd 理解成“内核交给用户程序的一张取物牌”：
 *   用户程序不需要知道文件在 ramdisk 哪个偏移、设备该调用哪个函数；
 *   它之后只把 fd 传回来，内核就能通过 get_file(fd) 找回完整 Finfo。
 *
 * 这里没有真正维护“每次 open 独立生成一个 fd 表项”的结构，
 * 因而同一个文件多次 open 会共享 Finfo.open_offset。
 * 对当前单进程 shell 演示足够，但它不是完整 Unix 文件描述符语义。
 */
static Finfo *get_file(int fd) {
  if (fd >= 0 && fd < NR_FILES) return &file_table[fd];
  fd -= NR_FILES;
  if (fd >= 0 && fd < RAMFS_MAX_FILES && ramfs[fd].name != NULL) {
    return &ramfs[fd];
  }
  return NULL;
}

/* 路径查找先查静态 ramdisk/设备/伪文件，再查运行期 ramfs 文件。 */
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

/*
 * 创建一个空的 ramfs 文件，只分配元数据，不立即分配数据页。
 * 真正的数据页在第一次写入、需要扩容时由 ramfs_grow() 申请。
 */
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

/*
 * ramfs 文件按页扩容。new_page() 是单调页分配器，目前没有 free_page()，
 * 所以扩容时旧 data 页不会被回收；这是课程项目里常见的简化实现。
 */
static bool ramfs_grow(Finfo *f, size_t need) {
  if (need <= f->capacity) return true;
  if (need > RAMFS_MAX_FILE_SIZE) {
    Log("ramfs_grow: %s — need=%u exceeds RAMFS_MAX_FILE_SIZE=%u",
        f->name, (unsigned)need, (unsigned)RAMFS_MAX_FILE_SIZE);
    return false;
  }

  size_t new_cap = ROUNDUP(need, PGSIZE);
  if (new_cap > RAMFS_MAX_FILE_SIZE) new_cap = RAMFS_MAX_FILE_SIZE;

  uint8_t *new_data = new_page(new_cap / PGSIZE);
  if (new_data == NULL) {
    Log("ramfs_grow: %s — new_page(%u pages) failed (need=%u, cap=%u)",
        f->name, (unsigned)(new_cap / PGSIZE), (unsigned)need, (unsigned)f->capacity);
    return false;
  }

  if (f->data != NULL && f->size > 0) {
    memcpy(new_data, f->data, f->size);
  }

  f->data = new_data;
  f->capacity = new_cap;
  return true;
}

/* 统计运行期 ramfs 已经占用的页容量，用于 /proc/meminfo 展示。 */
static size_t ramfs_used_capacity(void) {
  size_t used = 0;
  for (int i = 0; i < RAMFS_MAX_FILES; i++) {
    if (ramfs[i].name != NULL) used += ramfs[i].capacity;
  }
  return used;
}

static int ramfs_file_count(void) {
  int count = 0;
  for (int i = 0; i < RAMFS_MAX_FILES; i++) {
    if (ramfs[i].name != NULL) count++;
  }
  return count;
}

/*
 * /proc/processes 动态生成进程表，供 shell 的 ps 命令使用。
 */
static size_t proc_processes_read(void *buf, size_t offset, size_t len) {
  static char info[2048];
  int n = proc_get_process_list(info, sizeof(info));
  if (n < 0) return 0;
  return slice_read(info, (size_t)n, buf, offset, len);
}

/*
 * /proc/meminfo 不是磁盘文件，而是每次 read 时现场生成的文本。
 * shell 中 cat /proc/meminfo 看到的内存信息就来自这里。
 */
static size_t proc_meminfo_read(void *buf, size_t offset, size_t len) {
  size_t total = 0, used = 0, free = 0;
  get_memory_info(&total, &used, &free);

  char info[256];
  int n = snprintf(info, sizeof(info),
      "MemTotal: %u bytes\n"
      "MemUsed:  %u bytes\n"
      "MemFree:  %u bytes\n"
      "PageSize: %u bytes\n"
      "RamfsUsed: %u bytes\n"
      "RamfsFiles: %u/%u\n"
      "RamfsMaxFile: %u bytes\n",
      (unsigned)total, (unsigned)used, (unsigned)free,
      (unsigned)PGSIZE, (unsigned)ramfs_used_capacity(),
      (unsigned)ramfs_file_count(), (unsigned)RAMFS_MAX_FILES,
      (unsigned)RAMFS_MAX_FILE_SIZE);

  return slice_read(info, (size_t)n, buf, offset, len);
}

static size_t append_proc_line(char *buf, size_t cap, size_t pos,
    const char *fmt, ...) {
  if (pos >= cap) return cap;

  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf + pos, cap - pos, fmt, ap);
  va_end(ap);

  if (n <= 0) return pos;
  size_t wrote = (size_t)n;
  if (wrote >= cap - pos) return cap;
  return pos + wrote;
}

/*
 * /proc/files 同样是现场生成的文本，用来展示当前系统认识的文件：
 * 静态 ramdisk 文件、设备文件、伪文件，以及运行期创建的 ramfs 文件。
 */
static size_t proc_files_read(void *buf, size_t offset, size_t len) {
  static char info[PROC_FILES_BUFSIZE];
  size_t pos = 0;

  pos = append_proc_line(info, sizeof(info), pos, "type     size    path\n");
  for (int i = 0; i < NR_FILES && pos < sizeof(info); i++) {
    const char *type = "file";
    if (i == FD_STDIN || i == FD_STDOUT || i == FD_STDERR ||
        i == FD_EVENTS || i == FD_FB || i == FD_SB || i == FD_SBCTL) {
      type = "dev";
    } else if (i == FD_DISPINFO || i == FD_MEMINFO || i == FD_FILES) {
      type = "proc";
    }
    pos = append_proc_line(info, sizeof(info), pos, "%s %u %s\n",
        type, (unsigned)file_table[i].size, file_table[i].name);
  }

  for (int i = 0; i < RAMFS_MAX_FILES && pos < sizeof(info); i++) {
    if (ramfs[i].name != NULL) {
      pos = append_proc_line(info, sizeof(info), pos, "%s %u %s\n",
          "ramfs", (unsigned)ramfs[i].size, ramfs[i].name);
    }
  }

  if (pos > sizeof(info)) pos = sizeof(info);
  return slice_read(info, pos, buf, offset, len);
}

void init_fs() {
  /*
   * /dev/fb 的大小取决于 NEMU/AM 报告的屏幕分辨率。
   * SDL/NDL 最终会把图形应用的像素写到这个设备文件。
   */
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  file_table[FD_FB].size = cfg.width * cfg.height * sizeof(uint32_t);
}

/*
 * fs_open(pathname, flags, mode)
 *
 * 调用来源：SYS_open。
 * 参数含义：
 *   pathname: 用户程序想打开的路径，例如 "/bin/pal"、"/proc/meminfo"。
 *   flags: O_CREAT/O_TRUNC/O_APPEND 等打开方式。
 *   mode: 创建文件权限；当前 ramfs 不做权限检查，所以忽略。
 * 主要工作：
 *   先在静态 file_table 和运行期 ramfs 里找路径；
 *   找不到但带 O_CREAT 时，就创建一个空 ramfs 文件；
 *   根据 O_APPEND/O_TRUNC 初始化文件偏移和大小。
 *   最后把这个具体文件对应的 fd 返回给用户程序。
 * 返回值：
 *   成功返回 fd；失败返回 -1。
 *
 * 例子：
 *   open("/proc/meminfo") 返回的是 /proc/meminfo 那一项的 fd；
 *   open("/bin/pal") 返回的是 /bin/pal 那一项的 fd；
 *   后续 read(fd, ...) 会根据这个 fd 找回完全不同的 Finfo。
 */
int fs_open(const char *pathname, int flags, int mode) {
  (void)mode;
  /*
   * open 的返回值是 fd，不只是成功/失败布尔值：
   *   >= 0 表示后续 read/write/lseek/close 要继续使用的 fd；
   *   -1 表示路径不存在或无法创建。
   */
  int fd = find_file(pathname);
  if (fd < 0 && (flags & FS_O_CREAT)) {
    fd = alloc_ramfs_file(pathname);
  }

  Finfo *f = get_file(fd);
  if (f == NULL) return -1;

  /* O_TRUNC 只允许截断运行期 ramfs 文件，避免破坏 ramdisk 内置文件。 */
  if (f->dynamic && (flags & FS_O_TRUNC)) {
    f->size = 0;
  }

  /* O_APPEND 会让第一次写从文件末尾开始；否则从头开始。 */
  f->open_offset = (flags & FS_O_APPEND) ? f->size : 0;
  return fd;
}

/*
 * fs_read(fd, buf, len)
 *
 * 调用来源：SYS_read。
 * 参数含义：
 *   fd: fs_open 返回的文件描述符。
 *   buf: 用户态缓冲区，内核把读取结果复制到这里。
 *   len: 用户最多想读多少字节。
 * 主要工作：
 *   设备/伪文件走 read 回调；
 *   ramfs 文件从内存 data[] 读；
 *   静态 ramdisk 文件从 ramdisk.img 读；
 *   最后更新 open_offset。
 * 返回值：
 *   实际读取字节数；读到文件末尾返回 0；fd 无效返回 -1。
 *
 * 精确读哪个文件的关键不是“类型”，而是第一行 get_file(fd)。
 * 它把 fd 还原成某一个具体 Finfo，后面的分支再按这个 Finfo 的内容读：
 *   有 read 回调    -> 调用这个具体文件/设备的回调；
 *   dynamic=true    -> 从这个 Finfo 自己的 data 指针读；
 *   普通 ramdisk 文件 -> 从 disk_offset + open_offset 位置读。
 */
size_t fs_read(int fd, void *buf, size_t len) {
  Finfo *f = get_file(fd);
  if (f == NULL) return (size_t)-1;

  /*
   * /dev/events 每次读取都是查询键盘/输入事件，不使用文件偏移。
   * 如果把 open_offset 参与进去，事件流反而会被错误地“跳过”。
   */
  if (fd == FD_EVENTS) {
    return events_read(buf, 0, len);
  }

  /*
   * 有 read 回调的对象优先走回调，例如 /proc/meminfo、/proc/files、
   * /proc/dispinfo、/dev/sbctl。回调返回实际读到的字节数。
   */
  if (f->read != NULL && f->read != invalid_read) {
    size_t ret = f->read(buf, f->open_offset, len);
    f->open_offset += ret;
    return ret;
  }

  /*
   * 运行期 ramfs 文件直接从内存 data[] 里读。
   * 不同动态文件的 Finfo.data 不同，所以即使它们都是 ramfs 类型，
   * fd 找到的具体 Finfo 也能区分到底读哪一个文件。
   */
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

  /*
   * 静态 ramdisk 文件通过 disk_offset 映射到 ramdisk.img。
   * files.h 给每个内置文件生成不同的 disk_offset/size，
   * 因此同样调用 ramdisk_read()，也会读到不同文件的内容。
   */
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

/*
 * fs_write(fd, buf, len)
 *
 * 调用来源：SYS_write。
 * 参数含义：
 *   fd: 要写入的文件描述符。
 *   buf: 用户态数据地址。
 *   len: 要写入的字节数。
 * 主要工作：
 *   stdout/stderr 写串口；
 *   /dev/fb、/dev/sb 等设备走 write 回调；
 *   ramfs 文件必要时扩容，然后写入内存；
 *   静态 ramdisk 文件只保留有限的原地写语义。
 * 返回值：
 *   实际写入字节数；失败返回 -1。
 *
 * 精确写到哪里同样由 fd 决定：
 *   fd=1/2        -> serial_write，输出到宿主终端；
 *   fd=/dev/fb    -> fb_write，写屏幕像素；
 *   fd=/dev/sb    -> sb_write，写音频数据；
 *   fd=ramfs 文件 -> 写入这个文件自己的 data 区。
 */
size_t fs_write(int fd, const void *buf, size_t len) {
  Finfo *f = get_file(fd);
  if (f == NULL) return (size_t)-1;

  /*
   * stdout/stderr 写到串口。这里不维护 open_offset，
   * 因为终端输出是字符流，不是普通可 seek 文件。
   */
  if ((fd == FD_STDOUT || fd == FD_STDERR) &&
      f->write != NULL && f->write != invalid_write) {
    return f->write(buf, 0, len);
  }

  /* 设备文件优先走 write 回调，例如 /dev/fb、/dev/sb、/dev/sbctl。 */
  if (f->write != NULL && f->write != invalid_write) {
    size_t ret = f->write(buf, f->open_offset, len);
    f->open_offset += ret;
    return ret;
  }

  /* ramfs 文件可写且可扩容，这是 touch/write/append 能工作的基础。 */
  if (f->dynamic) {
    size_t end = f->open_offset + len;
    if (!ramfs_grow(f, end)) return (size_t)-1;

    memcpy(f->data + f->open_offset, buf, len);
    f->open_offset = end;
    if (end > f->size) f->size = end;
    return len;
  }

  /*
   * 静态 ramdisk 文件原则上不适合作为演示里的可写文件。
   * 这里保留 PA 原有的 bounded write 语义：最多写到文件原大小范围内。
   */
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

/*
 * fs_lseek(fd, offset, whence)
 *
 * 调用来源：SYS_lseek。
 * 参数含义：
 *   fd: 文件描述符。
 *   offset: 偏移量。
 *   whence: SEEK_SET 从开头算，SEEK_CUR 从当前位置算，SEEK_END 从文件末尾算。
 * 主要工作：
 *   计算新的 open_offset，并做边界检查。
 * 返回值：
 *   成功返回新的偏移；失败返回 -1。
 */
size_t fs_lseek(int fd, off_t offset, int whence) {
  Finfo *f = get_file(fd);
  if (f == NULL) return (size_t)-1;

  /* 音频控制/播放设备是流式设备，不支持用 lseek 改偏移。 */
  assert(fd != FD_SB && fd != FD_SBCTL);

  off_t new_offset = 0;
  switch (whence) {
    case SEEK_SET: new_offset = offset; break;
    case SEEK_CUR: new_offset = (off_t)f->open_offset + offset; break;
    case SEEK_END: new_offset = (off_t)f->size + offset; break;
    default: panic("fs_lseek: invalid whence = %d", whence);
  }

  if (new_offset < 0) return (size_t)-1;
  /* 静态文件不能 seek 到 EOF 之后；ramfs 可以在大小上限内留洞后写入。 */
  if (!f->dynamic && (size_t)new_offset > f->size) return (size_t)-1;
  if (f->dynamic && (size_t)new_offset > RAMFS_MAX_FILE_SIZE) return (size_t)-1;

  f->open_offset = (size_t)new_offset;
  return f->open_offset;
}

/*
 * fs_close(fd)
 *
 * 调用来源：SYS_close。
 * 当前简化点：
 *   没有真正的“打开文件实例”，所以 close 不释放资源，
 *   只判断 fd 是否能映射到一个 Finfo。
 * 返回值：
 *   成功 0，失败 -1。
 */
int fs_close(int fd) {
  /*
   * 当前没有真正的“打开文件表”，close 只检查 fd 是否有效。
   * 如果后续实现多进程/多 fd，需要把 open_offset 从 Finfo 拆到 fd 表中。
   */
  return get_file(fd) == NULL ? -1 : 0;
}

/*
 * fs_fstat(fd, buf)
 *
 * 调用来源：SYS_fstat。
 * 参数含义：
 *   fd: 文件描述符。
 *   buf: 用户态 struct stat 缓冲区。
 * 主要工作：
 *   填写 st_mode/st_size，让 libc/SDL 知道这是字符设备还是普通文件，
 *   以及普通文件的大小。
 * 返回值：
 *   成功 0，失败 -1。
 */
int fs_fstat(int fd, struct stat *buf) {
  Finfo *f = get_file(fd);
  if (f == NULL || buf == NULL) return -1;

  /*
   * fstat 主要服务于 libc/SDL/应用判断“这是字符设备还是普通文件”。
   * 设备文件报 S_IFCHR，ramdisk/ramfs/proc 文件在这里统一报 S_IFREG。
   */
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

/*
 * fs_unlink(pathname)
 *
 * 调用来源：SYS_unlink，也就是 shell 的 rm 或 libc 的 unlink。
 * 参数含义：
 *   pathname: 要删除的路径。
 * 主要工作：
 *   只允许删除运行期 ramfs 文件；
 *   静态 ramdisk 文件、设备文件、/proc 文件都拒绝删除。
 * 返回值：
 *   成功 0，失败 -1。
 */
int fs_unlink(const char *pathname) {
  /*
   * unlink 只删除运行期 ramfs 文件。
   * /bin/dterm、/bin/pal、资源文件等静态 ramdisk 内容是内核镜像的一部分，
   * 运行时不允许删除，否则 loader 和演示资源会被破坏。
   */
  int fd = find_file(pathname);
  if (fd < NR_FILES) return -1;

  Finfo *f = get_file(fd);
  if (f == NULL || !f->dynamic) return -1;

  int idx = fd - NR_FILES;
  if (f->data != NULL) {
    free_page(f->data);
  }
  ramfs_names[idx][0] = '\0';
  ramfs[idx] = (Finfo) {0};
  return 0;
}
