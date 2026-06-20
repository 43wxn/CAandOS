#include <common.h>
#include <am.h>
#include <fs.h>
#include <memory.h>
#include "syscall.h"
#include "proc.h"

/*
 * 系统调用从用户程序进入 nanos-lite 的完整路径：
 *
 *   navy-apps/libs/libos/src/syscall.c
 *     把系统调用号放入 a7，把最多 3 个参数放入 a0/a1/a2，然后执行 ecall。
 *
 *   abstract-machine/am/src/riscv/nemu/cte.c
 *     由 AM 的 trap 入口保存寄存器到 Context，并根据 mcause 判断异常来源。
 *     注意：mcause 才是“异常类型”；a7 只是用户程序传来的“系统调用号”。
 *
 *   nanos-lite/src/irq.c
 *     收到 EVENT_SYSCALL 后调用 do_syscall(c)。
 *
 *   nanos-lite/src/syscall.c
 *     从 Context 里取出 a7/a0/a1/a2，分发到真正的内核服务函数。
 *
 * RISC-V ABI 中 a0 既是第 0 个参数寄存器，也是返回值寄存器。
 * 所以这里最后写 c->GPRx，本质就是把系统调用返回值写回 a0，
 * trap 返回用户态后，libos 的内联汇编就能从 a0 读到返回值。
 */
typedef struct {
  uint32_t tv_sec;
  uint32_t tv_usec;
} user_timeval_t;

typedef struct {
  int32_t tz_minuteswest;
  int32_t tz_dsttime;
} user_timezone_t;

typedef struct {
  uint32_t tms_utime;
  uint32_t tms_stime;
  uint32_t tms_cutime;
  uint32_t tms_cstime;
} user_tms_t;

#define USER_CLK_TCK 100

static uint64_t uptime_us(void) {
  AM_TIMER_UPTIME_T uptime = io_read(AM_TIMER_UPTIME);
  return uptime.us;
}

static uint32_t uptime_sec(void) {
  return (uint32_t)(uptime_us() / 1000000);
}

static uint32_t uptime_ticks(void) {
  return (uint32_t)(uptime_us() * USER_CLK_TCK / 1000000);
}

void do_syscall(Context *c) {
  uintptr_t a[4];
  /*
   * 在 abstract-machine/am/include/arch/riscv.h 里：
   *   GPR1 = gpr[17] = a7，系统调用号，例如 SYS_open/SYS_read。
   *   GPR2 = gpr[10] = a0，第 1 个参数，同时也是返回值寄存器。
   *   GPR3 = gpr[11] = a1，第 2 个参数。
   *   GPR4 = gpr[12] = a2，第 3 个参数。
   *
   * 举例：
   *   open(path, flags, mode)  -> a7=SYS_open,  a0=path, a1=flags,  a2=mode
   *   read(fd, buf, count)     -> a7=SYS_read,  a0=fd,   a1=buf,    a2=count
   *   write(fd, buf, count)    -> a7=SYS_write, a0=fd,   a1=buf,    a2=count
   *   lseek(fd, off, whence)   -> a7=SYS_lseek, a0=fd,   a1=offset, a2=whence
   */
  a[0] = c->GPR1;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

  switch (a[0]) {
    case SYS_exit:
      /*
       * 当前项目仍是单进程演示模型。用户程序退出后，我们重新装入
       * DEFAULT_USER_PROGRAM，相当于回到“系统命令行”。
       */
      Log("SYS_exit(status=%d)", (int)a[1]);
      proc_exit_current((int)a[1]);
      panic("SYS_exit should not return");
      break;

    case SYS_yield:
      c->GPRx = 0;
      yield();
      break;

    case SYS_open:
      /*
       * open(path, flags, mode)
       *   a0/a[1] = 用户态字符串地址 path
       *   a1/a[2] = 打开标志 flags，例如 O_CREAT/O_APPEND
       *   a2/a[3] = mode，本项目基本不用
       * 返回值：成功返回 fd，失败返回 -1。
       *
       * 文件相关 syscall 只在这里做分发，真正的文件系统逻辑在 fs.c。
       * 这个 fd 会回到用户态 a0；用户程序之后 read/write 时再把它作为参数传回。
       */
      c->GPRx = fs_open((const char *)a[1], (int)a[2], (int)a[3]);
      break;

    case SYS_read:
      /*
       * read(fd, buf, count)
       *   a0/a[1] = fd
       *   a1/a[2] = 用户态缓冲区地址 buf
       *   a2/a[3] = 最多读取 count 字节
       * 返回值：实际读取字节数；0 表示 EOF；失败返回 -1。
       * fd 是 open 返回的具体句柄，fs_read 会用它定位到某个 Finfo。
       */
      c->GPRx = fs_read((int)a[1], (void *)a[2], (size_t)a[3]);
      break;

    case SYS_write:
      /*
       * write(fd, buf, count)
       *   a0/a[1] = fd
       *   a1/a[2] = 用户态数据地址 buf
       *   a2/a[3] = 写入 count 字节
       * 返回值：实际写入字节数；失败返回 -1。
       * fd 是 open 返回的具体句柄，fs_write 会用它定位到某个 Finfo。
       */
      c->GPRx = fs_write((int)a[1], (const void *)a[2], (size_t)a[3]);
      break;

    case SYS_close:
      /*
       * close(fd)
       *   a0/a[1] = fd
       * 返回值：成功 0，失败 -1。
       */
      c->GPRx = fs_close((int)a[1]);
      break;

    case SYS_lseek:
      /*
       * lseek(fd, offset, whence)
       *   a0/a[1] = fd
       *   a1/a[2] = 目标偏移 offset
       *   a2/a[3] = SEEK_SET/SEEK_CUR/SEEK_END
       * 返回值：新的文件偏移；失败返回 -1。
       */
      c->GPRx = fs_lseek((int)a[1], (off_t)a[2], (int)a[3]);
      break;

    case SYS_fstat:
      /*
       * fstat(fd, statbuf)
       *   a0/a[1] = fd
       *   a1/a[2] = 用户态 struct stat *statbuf
       * 返回值：成功 0，失败 -1。libc/SDL 会用它判断文件类型和大小。
       */
      c->GPRx = fs_fstat((int)a[1], (void *)a[2]);
      break;

    case SYS_brk:
      /*
       * Navy 的 malloc/newlib 会通过 sbrk/brk 申请堆空间。
       * mm_brk() 会检查用户态 brk 是否越界进入内核堆区域，
       * 防止用户 malloc 覆盖 new_page 分配的页面（fork 子进程上下文等）。
       * 返回 0 表示成功，-1 表示被拒绝（内存不足或越界）。
       */
      c->GPRx = mm_brk((uintptr_t)a[1]);
      break;

    case SYS_gettimeofday: {
      /* 把 AM 的微秒级 uptime 转成用户态 struct timeval。 */
      user_timeval_t *tv = (user_timeval_t *)a[1];
      user_timezone_t *tz = (user_timezone_t *)a[2];
      uint64_t us = uptime_us();

      if (tv != NULL) {
        tv->tv_sec = us / 1000000;
        tv->tv_usec = us % 1000000;
      }
      if (tz != NULL) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
      }
      c->GPRx = 0;
      break;
    }

    case SYS_time: {
      /*
       * time(time_t *tloc)
       * 当前没有 RTC 日历时间，因此返回“开机到现在的秒数”作为演示时间。
       * libc 的 time() 通常已经通过 gettimeofday 实现；这个分支用于补齐 syscall 表。
       */
      uint32_t sec = uptime_sec();
      uint32_t *tloc = (uint32_t *)a[1];
      if (tloc != NULL) *tloc = sec;
      c->GPRx = sec;
      break;
    }

    case SYS_signal:
      /*
       * signal(sig, handler)
       * 当前没有信号表和异步递送机制，保留接口并明确返回失败。
       * 后续实现 kill/signal 时，可在 PCB 中增加 signal handler 表。
       */
      c->GPRx = -1;
      break;

    case SYS_times: {
      /*
       * times(struct tms *buf)
       * 单进程模型下先用 uptime ticks 填充 user/system 时间。
       * 这不是精确 CPU 计时，但足够让测试程序看到 times syscall 可返回。
       */
      uint32_t ticks = uptime_ticks();
      user_tms_t *buf = (user_tms_t *)a[1];
      if (buf != NULL) {
        buf->tms_utime = ticks;
        buf->tms_stime = 0;
        buf->tms_cutime = 0;
        buf->tms_cstime = 0;
      }
      c->GPRx = ticks;
      break;
    }

    case SYS_kill:
      /*
       * kill(pid, sig)
       * 当前只支持 kill(getpid(), 0) 用作“进程存在性探测”。
       * 真正终止/信号处理需要多进程 PCB 和 signal handler 支撑。
       */
      c->GPRx = proc_kill((int)a[1], (int)a[2]);
      break;
   
    case SYS_execve:
      /*
       * execve 成功时会用新的 ELF 程序覆盖当前执行流，正常不会返回。
       * 如果 naive_uload 失败或未来改成可失败返回，再把 -1 传回用户态。
       */
      c->GPRx = proc_execve((const char *)a[1],
          (char *const *)a[2], (char *const *)a[3]);
      break;

    case SYS_getpid:
      /* 单进程模型下固定把当前进程看成 pid=1。 */
      c->GPRx = proc_getpid();
      break;


    case SYS_fork:
      c->GPRx = proc_fork(c);
      break;

    case SYS_link:
      /*
       * link(oldpath, newpath)
       * 当前文件系统没有 inode/引用计数模型，不支持硬链接。
       */
      c->GPRx = -1;
      break;

    case SYS_wait:
      /*
       * wait(int *status)
       * 当前没有子进程，保留接口给后续进程回收使用。
       */
      c->GPRx = proc_wait((int *)a[1]);
      break;

    case SYS_unlink:
      /* 删除运行期创建的 ramfs 文件；fs.c 会拒绝删除静态 ramdisk 文件。 */
      c->GPRx = fs_unlink((const char *)a[1]);
      break;

    case SYS_shutdown:
      halt((int)a[1]);
      break;

    case SYS_demo:
      c->GPRx = proc_start_demo();
      /* 将 demo 日志拷贝到用户态固定地址，shell 可直接读取 */
      {
        int n = proc_get_demo_log((char *)USER_DEMO_LOG_ADDR, 4096);
        if (n > 0) Log("SYS_demo: wrote %d bytes demo log to %p", n, (void *)USER_DEMO_LOG_ADDR);
      }
      break;

    default:
      panic("Unhandled syscall ID = %d", (int)a[0]);
  }
}
