#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>
#include "syscall.h"

#define _concat(x, y) x ## y
#define concat(x, y) _concat(x, y)
#define _args(n, list) concat(_arg, n) list
#define _arg0(a0, ...) a0
#define _arg1(a0, a1, ...) a1
#define _arg2(a0, a1, a2, ...) a2
#define _arg3(a0, a1, a2, a3, ...) a3
#define _arg4(a0, a1, a2, a3, a4, ...) a4
#define _arg5(a0, a1, a2, a3, a4, a5, ...) a5

#define SYSCALL _args(0, ARGS_ARRAY)
#define GPR1    _args(1, ARGS_ARRAY)
#define GPR2    _args(2, ARGS_ARRAY)
#define GPR3    _args(3, ARGS_ARRAY)
#define GPR4    _args(4, ARGS_ARRAY)
#define GPRx    _args(5, ARGS_ARRAY)

#if defined(__riscv)
# ifdef __riscv_e
#  define ARGS_ARRAY ("ecall", "a5", "a0", "a1", "a2", "a0")
# else
#  define ARGS_ARRAY ("ecall", "a7", "a0", "a1", "a2", "a0")
# endif
#else
# error unsupported ISA
#endif


__attribute__((noinline))
intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2) {
#if defined(__riscv)
  register intptr_t x10 asm("a0") = a0;
  register intptr_t x11 asm("a1") = a1;
  register intptr_t x12 asm("a2") = a2;
# ifdef __riscv_e
  register intptr_t x15 asm("a5") = type;
# else
  register intptr_t x17 asm("a7") = type;
# endif

  asm volatile (
    "ecall"
    : "+r"(x10)
# ifdef __riscv_e
    : "r"(x11), "r"(x12), "r"(x15)
# else
    : "r"(x11), "r"(x12), "r"(x17)
# endif
    : "memory",
      "a3", "a4", "a5", "a6",
      "t0", "t1", "t2", "t3", "t4", "t5", "t6"
  );

  return x10;
#else
# error unsupported ISA
#endif
}

void _exit(int status) {
  _syscall_(SYS_exit, status, 0, 0);
  while (1) {}
}

int _open(const char *path, int flags, mode_t mode) {
  if (path == NULL) {
    errno = EINVAL;
    return -1;
  }
  intptr_t ret = _syscall_(SYS_open, (intptr_t)path, flags, mode);
  // 系统调用返回值直接返回，不要在这里设置 errno
  // 因为负值可能是有效的文件描述符？实际上文件描述符不应该为负
  // 但根据日志，open 返回了 3，这是正常的，所以不要在这里判断 ret < 0
  // 只有在真正的错误时才返回 -1，但 nanos-lite 的 syscall.c 中 open 成功返回非负值
  // 失败返回 -1，所以这里直接返回 ret
  return (int)ret;
}

ssize_t _read(int fd, void *buf, size_t count) {
  if (buf == NULL && count != 0) {
    errno = EINVAL;
    return -1;
  }
  intptr_t ret = _syscall_(SYS_read, fd, (intptr_t)buf, count);
  // read 返回实际读取的字节数，可能为 0，失败时返回 -1
  // 直接返回 ret 即可
  return (ssize_t)ret;
}

ssize_t _write(int fd, const void *buf, size_t count) {
  if (buf == NULL && count != 0) {
    errno = EINVAL;
    return -1;
  }
  intptr_t ret = _syscall_(SYS_write, fd, (intptr_t)buf, count);
  return (ssize_t)ret;
}

int _close(int fd) {
  intptr_t ret = _syscall_(SYS_close, fd, 0, 0);
  return (int)ret;
}

off_t _lseek(int fd, off_t offset, int whence) {
  intptr_t ret = _syscall_(SYS_lseek, fd, (intptr_t)offset, whence);
  return (off_t)ret;
}

void *_sbrk(intptr_t increment) {
  extern char _end;
  static uintptr_t cur_brk = 0;

  if (cur_brk == 0) cur_brk = (uintptr_t)&_end;

  uintptr_t old_brk = cur_brk;
  uintptr_t new_brk = cur_brk + increment;

  intptr_t ret = _syscall_(SYS_brk, new_brk, 0, 0);
  if (ret == 0) {
    cur_brk = new_brk;
    return (void *)old_brk;
  }

  errno = ENOMEM;
  return (void *)-1;
}

int _fstat(int fd, struct stat *buf) {
  if (buf == NULL) {
    errno = EINVAL;
    return -1;
  }
  intptr_t ret = _syscall_(SYS_fstat, fd, (intptr_t)buf, 0);
  return (int)ret;
}

int _gettimeofday(struct timeval *tv, struct timezone *tz) {
  intptr_t ret = _syscall_(SYS_gettimeofday, (intptr_t)tv, (intptr_t)tz, 0);
  return (int)ret;
}

int _stat(const char *fname, struct stat *buf) {
  if (fname == NULL || buf == NULL) {
    errno = EINVAL;
    return -1;
  }

  int fd = _open(fname, 0, 0);
  if (fd < 0) return -1;

  int ret = _fstat(fd, buf);
  _close(fd);
  return ret;
}

/* 以下这些先保持占位 */
int _execve(const char *fname, char * const argv[], char *const envp[]) { errno = ENOSYS; return -1; }

int _kill(int pid, int sig) { errno = ENOSYS; return -1; }
pid_t _getpid() { return 1; }
pid_t _fork() { errno = ENOSYS; return -1; }
pid_t vfork() { errno = ENOSYS; return -1; }
int _link(const char *d, const char *n) { errno = ENOSYS; return -1; }
int _unlink(const char *n) { errno = ENOSYS; return -1; }
pid_t _wait(int *status) { errno = ENOSYS; return -1; }
clock_t _times(void *buf) { errno = ENOSYS; return (clock_t)-1; }
int pipe(int pipefd[2]) { errno = ENOSYS; return -1; }
int dup(int oldfd) { errno = ENOSYS; return -1; }
int dup2(int oldfd, int newfd) { errno = ENOSYS; return -1; }
unsigned int sleep(unsigned int seconds) { errno = ENOSYS; return 0; }
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) { errno = ENOSYS; return -1; }
int symlink(const char *target, const char *linkpath) { errno = ENOSYS; return -1; }
int ioctl(int fd, unsigned long request, ...) { errno = ENOSYS; return -1; }
