#include <common.h>
#include <am.h>
#include <fs.h>
#include <stdint.h>
#include "syscall.h"

// 注意: 这里不能直接用宿主机的 <sys/time.h>/<sys/stat.h> 结构体写用户内存,
// 否则在 riscv32 用户程序下很容易把栈/堆写坏.
// PA3 的用户程序是 32-bit, 因此这里手动定义一个兼容布局.
typedef struct {
  int32_t tv_sec;
  int32_t tv_usec;
} riscv32_timeval;

typedef struct {
  int32_t tz_minuteswest;
  int32_t tz_dsttime;
} riscv32_timezone;

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
      // 让用户态 libos 自己填充 struct stat, 避免宿主机/用户态 ABI 不一致
      c->GPRx = 0;
      break;

    case SYS_brk:
      c->GPRx = 0;
      break;

    case SYS_gettimeofday: {
      riscv32_timeval *tv = (riscv32_timeval *)a[1];
      riscv32_timezone *tz = (riscv32_timezone *)a[2];
      AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);

      if (tv != NULL) {
        tv->tv_sec  = (int32_t)(uptime.us / 1000000);
        tv->tv_usec = (int32_t)(uptime.us % 1000000);
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