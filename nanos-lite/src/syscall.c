#include <common.h>
#include <am.h>
#include <fs.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "syscall.h"

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

  switch (a[0]) {
    case SYS_exit:
      Log("SYS_exit(status=%d)", (int)a[1]);
      halt(a[1]);
      break;

    case SYS_yield:
      c->GPRx = 0;
      yield();
      break;

    case SYS_open:
      c->GPRx = fs_open((const char *)a[1], (int)a[2], (int)a[3]);
      break;

    case SYS_read:
      c->GPRx = fs_read((int)a[1], (void *)a[2], (size_t)a[3]);
      break;

    case SYS_write:
      c->GPRx = fs_write((int)a[1], (const void *)a[2], (size_t)a[3]);
      break;

    case SYS_close:
      c->GPRx = fs_close((int)a[1]);
      break;

    case SYS_lseek:
      c->GPRx = fs_lseek((int)a[1], (off_t)a[2], (int)a[3]);
      break;

    case SYS_fstat:
      c->GPRx = fs_fstat((int)a[1], (struct stat *)a[2]);
      break;

    case SYS_brk:
      c->GPRx = 0;
      break;

    case SYS_gettimeofday: {
      struct timeval *tv = (struct timeval *)a[1];
      struct timezone *tz = (struct timezone *)a[2];
      AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);

      if (tv != NULL) {
        tv->tv_sec = uptime.us / 1000000;
        tv->tv_usec = uptime.us % 1000000;
      }
      if (tz != NULL) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
      }
      c->GPRx = 0;
      break;
    }

    case SYS_time:
    case SYS_signal:
    case SYS_times:
    case SYS_kill:
    case SYS_getpid:
    case SYS_execve:
    case SYS_fork:
    case SYS_link:
    case SYS_unlink:
    case SYS_wait:
      c->GPRx = -1;
      break;

    default:
      panic("Unhandled syscall ID = %d", (int)a[0]);
  }
}