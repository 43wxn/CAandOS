#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>
#include "syscall.h"

__attribute__((noinline))
intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2) {
#if defined(__riscv)
  register intptr_t x10 asm("a0") = a0;
  register intptr_t x11 asm("a1") = a1;
  register intptr_t x12 asm("a2") = a2;
  register intptr_t x17 asm("a7") = type;

  asm volatile (
    "ecall"
    : "+r"(x10)
    : "r"(x11), "r"(x12), "r"(x17)
    : "memory"
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
  return (int)_syscall_(SYS_open, (intptr_t)path, flags, mode);
}

ssize_t _read(int fd, void *buf, size_t count) {
  if (buf == NULL && count != 0) {
    errno = EINVAL;
    return -1;
  }
  return (ssize_t)_syscall_(SYS_read, fd, (intptr_t)buf, count);
}

ssize_t _write(int fd, const void *buf, size_t count) {
  if (buf == NULL && count != 0) {
    errno = EINVAL;
    return -1;
  }
  return (ssize_t)_syscall_(SYS_write, fd, (intptr_t)buf, count);
}

int _close(int fd) {
  return (int)_syscall_(SYS_close, fd, 0, 0);
}

off_t _lseek(int fd, off_t offset, int whence) {
  return (off_t)_syscall_(SYS_lseek, fd, (intptr_t)offset, whence);
}

void *_sbrk(intptr_t increment) {
  extern char _end;
  static uintptr_t cur_brk = 0;

  if (cur_brk == 0) {
    cur_brk = (uintptr_t)&_end;
  }

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
  return (int)_syscall_(SYS_fstat, fd, (intptr_t)buf, 0);
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

int _gettimeofday(struct timeval *tv, struct timezone *tz) {
  return (int)_syscall_(SYS_gettimeofday, (intptr_t)tv, (intptr_t)tz, 0);
}

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