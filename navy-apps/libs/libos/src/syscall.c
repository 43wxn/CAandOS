#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>
#include "syscall.h"

intptr_t _syscall_(intptr_t type, intptr_t arg0, intptr_t arg1, intptr_t arg2) {
  intptr_t ret;
  asm volatile (
    "mv a7, %1\n"
    "mv a0, %2\n"
    "mv a1, %3\n"
    "mv a2, %4\n"
    "ecall\n"
    "mv %0, a0\n"
    : "=r"(ret)
    : "r"(type), "r"(arg0), "r"(arg1), "r"(arg2)
    : "a0", "a1", "a2", "a7", "memory"
  );
  return ret;
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
  return (off_t)_syscall_(SYS_lseek, fd, offset, whence);
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
  return (int)_syscall_(SYS_fstat, fd, (intptr_t)buf, 0);
}

int _gettimeofday(struct timeval *tv, struct timezone *tz) {
  return (int)_syscall_(SYS_gettimeofday, (intptr_t)tv, (intptr_t)tz, 0);
}

/* 以下这些先保持占位 */
int _execve(const char *fname, char * const argv[], char *const envp[]) { errno = ENOSYS; return -1; }
int _stat(const char *fname, struct stat *buf) { errno = ENOSYS; return -1; }
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