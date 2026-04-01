#include <common.h>
#include <am.h>
#include <fs.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "syscall.h"

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;  // syscall no
  a[1] = c->GPR2;  // arg0
  a[2] = c->GPR3;  // arg1
  a[3] = c->GPR4;  // arg2

  switch (a[0]) {
    case SYS_exit:
      Log("SYS_exit(status=%d)", a[1]);
      halt(a[1]);
      break;

    case SYS_yield:
      Log("SYS_yield()");
      c->GPRx = 0;
      yield();
      break;

    case SYS_open:
      Log("SYS_open(path=%p:\"%s\", flags=%d, mode=%d)",
          (void *)a[1], (char *)a[1], (int)a[2], (int)a[3]);
      c->GPRx = fs_open((const char *)a[1], a[2], a[3]);
      Log("SYS_open -> %d", (int)c->GPRx);
      break;

    case SYS_read:
      Log("SYS_read(fd=%d, buf=%p, len=%d)", (int)a[1], (void *)a[2], (int)a[3]);
      c->GPRx = fs_read(a[1], (void *)a[2], a[3]);
      Log("SYS_read -> %d", (int)c->GPRx);
      break;

    case SYS_write:
      Log("SYS_write(fd=%d, buf=%p, len=%d)", (int)a[1], (void *)a[2], (int)a[3]);
      c->GPRx = fs_write(a[1], (const void *)a[2], a[3]);
      Log("SYS_write -> %d", (int)c->GPRx);
      break;

    case SYS_close:
      Log("SYS_close(fd=%d)", (int)a[1]);
      c->GPRx = fs_close(a[1]);
      Log("SYS_close -> %d", (int)c->GPRx);
      break;

    case SYS_lseek:
      Log("SYS_lseek(fd=%d, offset=%d, whence=%d)", (int)a[1], (int)a[2], (int)a[3]);
      c->GPRx = fs_lseek(a[1], (off_t)a[2], a[3]);
      Log("SYS_lseek -> %d", (int)c->GPRx);
      break;

    case SYS_fstat:
      Log("SYS_fstat(fd=%d, buf=%p)", (int)a[1], (void *)a[2]);
      c->GPRx = fs_fstat(a[1], (struct stat *)a[2]);
      Log("SYS_fstat -> %d", (int)c->GPRx);
      break;

    case SYS_brk:
      Log("SYS_brk(brk=%p)", (void *)a[1]);
      c->GPRx = 0;
      break;

    case SYS_gettimeofday: {
      struct timeval *tv = (struct timeval *)a[1];
      struct timezone *tz = (struct timezone *)a[2];
      AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);

      if (tv != NULL) {
        tv->tv_sec  = uptime.us / 1000000;
        tv->tv_usec = uptime.us % 1000000;
      }
      if (tz != NULL) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
      }
      c->GPRx = 0;
      break;
    }

    default:
      panic("Unhandled syscall ID = %d", (int)a[0]);
  }
}