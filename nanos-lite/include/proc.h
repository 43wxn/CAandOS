#ifndef __PROC_H__
#define __PROC_H__

#include <common.h>

/*
 * 默认用户态入口。它是一个被打包进 ramdisk 的 ELF 程序，
 * 负责提供类似 server shell 的交互界面。
 */
#define DEFAULT_USER_PROGRAM "/bin/dterm"
#define PROC_SINGLE_PID 1
#define USER_DEMO_LOG_ADDR 0x8FFE0000

typedef enum {
  PROC_UNUSED = 0,
  PROC_RUNNABLE,
  PROC_RUNNING,
  PROC_ZOMBIE,
} ProcState;

typedef struct PCB {
  Context *cp;
  Area stack;
  int pid;
  int ppid;
  ProcState state;
  const char *name;
  int exit_status;
} PCB;

extern PCB *current;

void switch_boot_pcb();
void init_proc();
Context* schedule(Context *prev);
void naive_uload(PCB *pcb, const char *filename, int argc, char *const argv[]);

/*
 * 当前阶段仍是单进程模型，但先把未来多任务/线程调度需要的接口列出来。
 * 后续真正实现 PCB 队列、上下文切换和时间片轮转时，优先替换这些函数内部。
 */
int proc_getpid(void);
const char *proc_get_name(void);
void proc_exit_current(int status);
int proc_execve(const char *filename, char *const argv[], char *const envp[]);
int proc_fork(Context *c);
int proc_wait(int *status);
int proc_kill(int pid, int sig);
int proc_create_thread(const char *name, void (*entry)(void *), void *arg);
void proc_thread_exit(int status);
Context *proc_schedule(Context *prev);
int proc_get_process_list(char *buf, size_t bufsz);
int proc_start_demo(void);
int proc_get_demo_log(char *buf, size_t bufsz);

#endif
