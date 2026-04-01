#include <common.h>
#include <am.h>
#include <fs.h>
#include <sys/stat.h>
#include "syscall.h"

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

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
          (void *)a[1], (char *)a[1], a[2], a[3]);
      c->GPRx = fs_open((const char *)a[1], a[2], a[3]);
      Log("SYS_open -> %d", c->GPRx);
      break;

    case SYS_read:
      Log("SYS_read(fd=%d, buf=%p, len=%d)", a[1], (void *)a[2], a[3]);
      c->GPRx = fs_read(a[1], (void *)a[2], a[3]);
      Log("SYS_read -> %d", c->GPRx);
      break;

    case SYS_write:
      Log("SYS_write(fd=%d, buf=%p, len=%d)", a[1], (void *)a[2], a[3]);
      c->GPRx = fs_write(a[1], (const void *)a[2], a[3]);
      Log("SYS_write -> %d", c->GPRx);
      break;

    case SYS_close:
      Log("SYS_close(fd=%d)", a[1]);
      c->GPRx = fs_close(a[1]);
      Log("SYS_close -> %d", c->GPRx);
      break;

    case SYS_lseek:
      Log("SYS_lseek(fd=%d, offset=%d, whence=%d)", a[1], a[2], a[3]);
      c->GPRx = fs_lseek(a[1], (off_t)a[2], a[3]);
      Log("SYS_lseek -> %d", c->GPRx);
      break;

    case SYS_fstat:
      Log("SYS_fstat(fd=%d, buf=%p)", a[1], (void *)a[2]);
      c->GPRx = fs_fstat(a[1], (struct stat *)a[2]);
      Log("SYS_fstat -> %d", c->GPRx);
      break;

    case SYS_brk:
      Log("SYS_brk(brk=%p)", (void *)a[1]);
      c->GPRx = 0;
      break;

    default:
      panic("Unhandled syscall ID = %d", a[0]);
  }
}