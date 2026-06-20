#include <proc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <arch/riscv.h>

void naive_uload(PCB *pcb, const char *filename, int argc, char *const argv[]);

#define MAX_NR_PROC 8

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static char current_name[128] = DEFAULT_USER_PROGRAM;
static PCB pcb_boot = {
  .pid = PROC_SINGLE_PID,
  .ppid = 0,
  .state = PROC_RUNNING,
  .name = current_name,
};
PCB *current = NULL;

static void proc_set_name(const char *name) {
  if (name == NULL || name[0] == '\0') name = DEFAULT_USER_PROGRAM;
  strncpy(current_name, name, sizeof(current_name) - 1);
  current_name[sizeof(current_name) - 1] = '\0';
  if (current != NULL) current->name = current_name;
}

void switch_boot_pcb() {
  current = &pcb_boot;
  proc_set_name(DEFAULT_USER_PROGRAM);
}

void init_proc() {
  switch_boot_pcb();
  Log("Initializing processes...");

  // 填充 PCB 数组以支持 ps 多进程显示
  pcb[0].pid = 0;
  pcb[0].ppid = 0;
  pcb[0].state = PROC_RUNNING;  /* idle — 仅供 ps 展示，调度器跳过 RUNNING 状态 */
  pcb[0].name = "[idle]";

  pcb[1].pid = 1;
  pcb[1].ppid = 0;
  pcb[1].state = PROC_RUNNING;  /* init — 仅供 ps 展示，不参与调度 */
  pcb[1].name = "[init]";

  proc_execve(DEFAULT_USER_PROGRAM, NULL, NULL);
  panic("init_proc: failed to load %s", DEFAULT_USER_PROGRAM);
}

int proc_getpid(void) {
  return current != NULL && current->pid > 0 ? current->pid : PROC_SINGLE_PID;
}

const char *proc_get_name(void) {
  return current != NULL && current->name != NULL ? current->name : DEFAULT_USER_PROGRAM;
}

void proc_exit_current(int status) {
  if (current != NULL) {
    current->exit_status = status;
    current->state = PROC_ZOMBIE;
  }

  /*
   * 单进程演示模型：用户程序退出后，重新装入默认 shell。
   * 真正多进程实现时，这里应改成把当前 PCB 标记为 ZOMBIE，
   * 再 schedule() 到其他可运行进程。
   */
  proc_execve(DEFAULT_USER_PROGRAM, NULL, NULL);
  panic("proc_exit_current: failed to reload %s", DEFAULT_USER_PROGRAM);
}

/* No change — NULL argv defaults as before */

int proc_execve(const char *filename, char *const argv[], char *const envp[]) {
  (void)envp;

  if (filename == NULL || filename[0] == '\0') return -1;

  if (current != NULL) {
    proc_set_name(filename);
    current->state = PROC_RUNNING;
  }

  /* Count argc from argv array */
  int argc = 0;
  if (argv != NULL) {
    while (argv[argc] != NULL) argc++;
  }

  naive_uload(NULL, filename, argc, argv);
  return -1;
}

static int next_pid = PROC_SINGLE_PID + 1;

/*
 * 子进程栈在 init_proc 时预分配，避免与用户态 malloc / ELF 加载冲突。
 */
#define CHILD_STACK_PAGES 4
static void *child_stack_base[MAX_NR_PROC];
static bool stacks_ready = false;

int proc_fork(Context *c) {
  /* 找空闲 PCB 槽位 */
  int slot = -1;
  for (int i = 0; i < MAX_NR_PROC; i++) {
    if (pcb[i].state == PROC_UNUSED) { slot = i; break; }
  }
  if (slot < 0) return -1;

  if (!stacks_ready) {
    /* 首次 fork 时预分配所有子进程栈 */
    for (int i = 0; i < MAX_NR_PROC; i++) {
      child_stack_base[i] = new_page(CHILD_STACK_PAGES);
      if (child_stack_base[i] == NULL) return -1;
    }
    stacks_ready = true;
  }

  Area stack = {
    .start = child_stack_base[slot],
    .end   = (void *)((uintptr_t)child_stack_base[slot] + CHILD_STACK_PAGES * PGSIZE),
  };

  /*
   * 子进程上下文放在栈底（接近 stack.start），远离栈顶。
   * 栈从 stack.end 向下增长，不会覆盖底部的上下文。
   * 使用逐字段拷贝而非 memcpy，避免 sizeof(Context) 可能不一致的问题。
   */
  Context *child_cp = (Context *)stack.start;
  /* 逐字段复制，与 proc_create_thread 一致，避开 memcpy 的对齐/padding 问题 */
  for (int i = 0; i < NR_REGS; i++) child_cp->gpr[i] = c->gpr[i];
  child_cp->mcause  = c->mcause;
  child_cp->mstatus = c->mstatus;
  child_cp->mepc    = c->mepc;
  child_cp->pdir    = NULL;
  child_cp->gpr[2]  = (uintptr_t)stack.end;  /* 独立栈 */
  child_cp->GPRx    = 0;                      /* fork() 返回 0 */
  Log("proc_fork: child_cp=%p child_mepc=%p child_sp=%p",
      child_cp, (void *)child_cp->mepc, (void *)child_cp->gpr[2]);

  /* 初始化 PCB */
  pcb[slot] = (PCB) {
    .cp = child_cp,
    .stack = stack,
    .pid = next_pid++,
    .ppid = current != NULL ? current->pid : 0,
    .state = PROC_RUNNABLE,
    .name = current != NULL ? current->name : "[child]",
    .exit_status = 0,
  };

  Log("proc_fork: parent=%d child=%d c=%p mepc=%p sp=%p",
      current ? current->pid : 0, pcb[slot].pid,
      c, (void *)c->mepc, (void *)c->gpr[2]);
  return pcb[slot].pid;
}

int proc_wait(int *status) {
  /*
   * 预留接口：未来应等待子进程 ZOMBIE 并回收 PCB。
   * 当前没有子进程；如果调用者提供 status，给出最近一次退出码供调试。
   */
  if (status != NULL && current != NULL) *status = current->exit_status;
  return -1;
}

int proc_kill(int pid, int sig) {
  /*
   * 预留接口：当前只支持 kill(pid, 0) 作为“进程是否存在”的探测。
   * 真正信号递送/终止进程留给后续多任务阶段。
   */
  if (pid == proc_getpid() && sig == 0) return 0;
  return -1;
}

int proc_create_thread(const char *name, void (*entry)(void *), void *arg) {
  /* 找空闲 PCB */
  int slot = -1;
  for (int i = 0; i < MAX_NR_PROC; i++) {
    if (pcb[i].state == PROC_UNUSED) { slot = i; break; }
  }
  if (slot < 0) return -1;

  /* 分配线程栈（4 页 = 16KB） */
  void *stack = new_page(4);
  if (stack == NULL) return -1;
  uintptr_t sp = (uintptr_t)stack + 4 * PGSIZE;

  /* 把 Context 放在栈底，栈从 sp 向下增长 */
  Context *ctx = (Context *)stack;
  memset(ctx, 0, sizeof(*ctx));
  ctx->mepc    = (uintptr_t)entry;
  ctx->mstatus = 0x1880;       /* MPP=3 (machine mode) to match ecall handler */
  ctx->gpr[2]  = sp;           /* 栈指针 */
  ctx->gpr[10] = (uintptr_t)arg; /* a0 = arg */

  pcb[slot] = (PCB) {
    .cp    = ctx,
    .stack = { .start = stack, .end = (void *)sp },
    .pid   = next_pid++,
    .ppid  = current ? current->pid : 0,
    .state = PROC_RUNNABLE,
    .name  = name,
  };

  Log("proc_create_thread: slot=%d pid=%d name=%s entry=%p sp=%p",
      slot, pcb[slot].pid, name, entry, (void *)sp);
  return pcb[slot].pid;
}

void proc_thread_exit(int status) {
  proc_exit_current(status);
}

Context *proc_schedule(Context *prev) {
  if (current != NULL) {
    current->cp = prev;
    if (current->state == PROC_RUNNING) current->state = PROC_RUNNABLE;
  }

  /* Round-robin: 从 PCB 数组中找下一个 RUNNABLE 进程 */
  static int last_idx = 0;
  for (int n = 0; n < MAX_NR_PROC; n++) {
    int i = (last_idx + n) % MAX_NR_PROC;
    if (pcb[i].state == PROC_RUNNABLE) {
      last_idx = (i + 1) % MAX_NR_PROC;
      pcb[i].state = PROC_RUNNING;
      PCB *next = &pcb[i];
      Log("proc_schedule: found pid=%d cp=%p mepc=%p sp=%p",
          next->pid, next->cp,
          (void *)next->cp->mepc, (void *)next->cp->gpr[2]);
      if (current != next) {
        current = next;
        return next->cp;
      }
      return prev; /* same process, no switch */
    }
  }

  /* 没有其他可运行进程，继续当前进程 */
  if (current != NULL) current->state = PROC_RUNNING;
  return prev;
}

Context* schedule(Context *prev) {
  return proc_schedule(prev);
}

/* ---- 多线程演示：仅用于 ps 展示和调度器验证 ---- */
static void demo_thread(void *arg) {
  while (1) yield();
}

int proc_start_demo(void) {
  next_pid = 10;
  int a = proc_create_thread("logger",  demo_thread, NULL);
  int b = proc_create_thread("worker",  demo_thread, NULL);
  int c = proc_create_thread("watchdog", demo_thread, NULL);
  Log("demo: logger=%d worker=%d watchdog=%d", a, b, c);
  return (a > 0 && b > 0 && c > 0) ? 0 : -1;
}

static int proc_append(char *buf, size_t bufsz, int pos, const char *fmt, ...) {
  if ((size_t)pos >= bufsz) return pos;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf + pos, bufsz - (size_t)pos, fmt, ap);
  va_end(ap);
  return n > 0 ? pos + n : pos;
}

int proc_get_process_list(char *buf, size_t bufsz) {
  int pos = 0;
  pos = proc_append(buf, bufsz, pos, "PID  PPID STATE      COMMAND\n");

  // 遍历所有 PCB 条目：idle → init → current → 其他
  for (int i = 0; i < MAX_NR_PROC; i++) {
    if (pcb[i].state == PROC_UNUSED && &pcb[i] != current) continue;

    PCB *p = &pcb[i];
    if (p->state == PROC_UNUSED) continue;

    const char *s = "???";
    switch (p->state) {
      case PROC_RUNNING:  s = "running";  break;
      case PROC_RUNNABLE: s = "runnable"; break;
      case PROC_ZOMBIE:   s = "zombie";   break;
      default: break;
    }
    const char *nm = p->name;
    if (nm == NULL || (uintptr_t)nm < 0x80000000) nm = "(unknown)";
    pos = proc_append(buf, bufsz, pos, "%d %d %s %s\n",
        p->pid, p->ppid, s, nm);
  }

  // 还有 pcb_boot（实际的 dterm 进程）
  if (current != NULL) {
    const char *s = "running";
    if (current->state == PROC_ZOMBIE) s = "zombie";
    const char *cnm = current->name;
    if (cnm == NULL || (uintptr_t)cnm < 0x80000000) cnm = "(shell)";
    pos = proc_append(buf, bufsz, pos, "%d %d %s %s\n",
        current->pid, current->ppid, s, cnm);
  }

  return pos;
}
