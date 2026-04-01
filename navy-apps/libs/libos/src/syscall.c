#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include "syscall.h"

__attribute__((noinline))
static intptr_t do_syscall(intptr_t type, intptr_t arg0, intptr_t arg1, intptr_t arg2) {
  register intptr_t a0 asm("a0") = arg0;
  register intptr_t a1 asm("a1") = arg1;
  register intptr_t a2 asm("a2") = arg2;
  register intptr_t a7 asm("a7") = type;
  asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2) {
  return do_syscall(type, a0, a1, a2);
}

void _exit(int status) {
  _syscall_(SYS_exit, status, 0, 0);
  while(1);
}

int _open(const char *path, int flags, mode_t mode) {
  return _syscall_(SYS_open, (intptr_t)path, flags, mode);
}

ssize_t _read(int fd, void *buf, size_t count) {
  return _syscall_(SYS_read, fd, (intptr_t)buf, count);
}

ssize_t _write(int fd, const void *buf, size_t count) {
  return _syscall_(SYS_write, fd, (intptr_t)buf, count);
}

int _close(int fd) {
  return _syscall_(SYS_close, fd, 0, 0);
}

off_t _lseek(int fd, off_t offset, int whence) {
  return _syscall_(SYS_lseek, fd, offset, whence);
}

void *_sbrk(intptr_t increment) {
  extern char _end;
  static uintptr_t brk;
  if (brk == 0) brk = (uintptr_t)&_end;
  brk += increment;
  return (void*)(brk - increment);
}

int _fstat(int fd, struct stat *buf) {
  return _syscall_(SYS_fstat, fd, (intptr_t)buf, 0);
}

int _gettimeofday(struct timeval *tv, struct timezone *tz) {
  return _syscall_(SYS_gettimeofday, (intptr_t)tv, (intptr_t)tz, 0);
}

int _execve(const char *fname, char *const argv[], char *const envp[]) { return -1; }
int _stat(const char *fname, struct stat *buf) { return -1; }
int _kill(int pid, int sig) { return -1; }
pid_t _getpid() { return 1; }
pid_t _fork() { return -1; }
int _link(const char *d, const char *n) { return -1; }
int _unlink(const char *n) { return -1; }
pid_t _wait(int *status) { return -1; }