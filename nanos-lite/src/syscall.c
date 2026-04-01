#include <common.h>
#include <am.h>
#include <fs.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "syscall.h"

typedef struct {
  int32_t tv_sec;
  int32_t tv_usec;
} riscv32_timeval;

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

  switch (a[0]) {
    case SYS_exit:
      while(1); // 安全退出，不崩溃
      break;
    case SYS_yield:
      c->GPRx = 0;
      yield();
      break;
    case SYS_open:
      c->GPRx = fs_open((const char*)a[1], a[2], a[3]);
      break;
    case SYS_read:
      c->GPRx = fs_read(a[1], (void*)a[2], a[3]);
      break;
    case SYS_write:
      c->GPRx = fs_write(a[1], (const void*)a[2], a[3]);
      break;
    case SYS_close:
      c->GPRx = fs_close(a[1]);
      break;
    case SYS_lseek:
      c->GPRx = fs_lseek(a[1], a[2], a[3]);
      break;
    case SYS_fstat:
      c->GPRx = fs_fstat(a[1], (struct stat*)a[2]);
      break;
    case SYS_brk:
      c->GPRx = 0;
      break;
    case SYS_gettimeofday: {
      riscv32_timeval *tv = (riscv32_timeval*)a[1];
      AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);
      tv->tv_sec = uptime.us / 1000000;
      tv->tv_usec = uptime.us % 1000000;
      c->GPRx = 0;
      break;
    }
    case 13: // SYS_rt_sigreturn
      c->GPRx = 0;
      break;
    default:
      c->GPRx = -1;
      break;
  }
}