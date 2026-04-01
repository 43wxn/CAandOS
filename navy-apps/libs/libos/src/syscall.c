#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "syscall.h"

// helper macros
#define _concat(x, y) x ## y
#define concat(x, y) _concat(x, y)
#define _args(n, list) concat(_arg, n) list
#define _arg0(a0, ...) a0
#define _arg1(a0, a1, ...) a1
#define _arg2(a0, a1, a2, ...) a2
#define _arg3(a0, a1, a2, a3, ...) a3
#define _arg4(a0, a1, a2, a3, a4, ...) a4
#define _arg5(a0, a1, a2, a3, a4, a5, ...) a5

// extract an argument from the macro array
#define SYSCALL  _args(0, ARGS_ARRAY)
#define GPR1 _args(1, ARGS_ARRAY)
#define GPR2 _args(2, ARGS_ARRAY)
#define GPR3 _args(3, ARGS_ARRAY)
#define GPR4 _args(4, ARGS_ARRAY)
#define GPRx _args(5, ARGS_ARRAY)

// ISA-dependent definitions
#if defined(__ISA_X86__)
# define ARGS_ARRAY ("int $0x80", "eax", "ebx", "ecx", "edx", "eax")
#elif defined(__ISA_MIPS32__)
# define ARGS_ARRAY ("syscall", "v0", "a0", "a1", "a2", "v0")
#elif defined(__riscv)
#ifdef __riscv_e
# define ARGS_ARRAY ("ecall", "a5", "a0", "a1", "a2", "a0")
#else
# define ARGS_ARRAY ("ecall", "a7", "a0", "a1", "a2", "a0")
#endif
#elif defined(__ISA_AM_NATIVE__)
# define ARGS_ARRAY ("call *0x100000", "rdi", "rsi", "rdx", "rcx", "rax")
#elif defined(__ISA_X86_64__)
# define ARGS_ARRAY ("int $0x80", "rdi", "rsi", "rdx", "rcx", "rax")
#elif defined(__ISA_LOONGARCH32R__)
# define ARGS_ARRAY ("syscall 0", "a7", "a0", "a1", "a2", "a0")
#else
#error _syscall_ is not implemented
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "syscall.h"

__attribute__((noinline))
static intptr_t do_syscall(intptr_t type, intptr_t arg0, intptr_t arg1, intptr_t arg2) {
  register intptr_t a0 asm("a0") = arg0;
  register intptr_t a1 asm("a1") = arg1;
  register intptr_t a2 asm("a2") = arg2;
  register intptr_t a7 asm("a7") = type;

  asm volatile (
    "ecall"
    : "+r"(a0)
    : "r"(a1), "r"(a2), "r"(a7)
    : "memory",
      "a3", "a4", "a5", "a6",
      "t0", "t1", "t2", "t3", "t4", "t5", "t6"
  );

  return a0;
}

intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2) {
  return do_syscall(type, a0, a1, a2);
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

int _gettimeofday(struct timeval *tv, struct timezone *tz) {
  (void)tv;
  (void)tz;
  errno = ENOSYS;
  return -1;
}

int _execve(const char *fname, char * const argv[], char *const envp[]) {
  (void)fname;
  (void)argv;
  (void)envp;
  errno = ENOSYS;
  return -1;
}

// Syscalls below are not used in Nanos-lite.
// But to pass linking, they are defined as dummy functions.

int _stat(const char *fname, struct stat *buf) {
  (void)fname;
  (void)buf;
  errno = ENOSYS;
  return -1;
}

int _kill(int pid, int sig) {
  (void)pid;
  (void)sig;
  errno = ENOSYS;
  return -1;
}

pid_t _getpid() {
  return 1;
}

pid_t _fork() {
  errno = ENOSYS;
  return -1;
}

pid_t vfork() {
  errno = ENOSYS;
  return -1;
}

int _link(const char *d, const char *n) {
  (void)d;
  (void)n;
  errno = ENOSYS;
  return -1;
}

int _unlink(const char *n) {
  (void)n;
  errno = ENOSYS;
  return -1;
}

pid_t _wait(int *status) {
  (void)status;
  errno = ENOSYS;
  return -1;
}

clock_t _times(void *buf) {
  (void)buf;
  errno = ENOSYS;
  return (clock_t)-1;
}

int pipe(int pipefd[2]) {
  (void)pipefd;
  errno = ENOSYS;
  return -1;
}

int dup(int oldfd) {
  (void)oldfd;
  errno = ENOSYS;
  return -1;
}

int dup2(int oldfd, int newfd) {
  (void)oldfd;
  (void)newfd;
  errno = ENOSYS;
  return -1;
}

unsigned int sleep(unsigned int seconds) {
  (void)seconds;
  errno = ENOSYS;
  return 0;
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
  (void)pathname;
  (void)buf;
  (void)bufsiz;
  errno = ENOSYS;
  return -1;
}

int symlink(const char *target, const char *linkpath) {
  (void)target;
  (void)linkpath;
  errno = ENOSYS;
  return -1;
}

int ioctl(int fd, unsigned long request, ...) {
  (void)fd;
  (void)request;
  errno = ENOSYS;
  return -1;
}