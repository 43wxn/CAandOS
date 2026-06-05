#include <proc.h>

void naive_uload(PCB *pcb, const char *filename);

#define MAX_NR_PROC 4

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

int proc_execve(const char *filename, char *const argv[], char *const envp[]) {
  (void)argv;
  (void)envp;

  if (filename == NULL || filename[0] == '\0') return -1;

  if (current != NULL) {
    proc_set_name(filename);
    current->state = PROC_RUNNING;
  }

  naive_uload(NULL, filename);
  return -1;
}

int proc_fork(void) {
  /*
   * 预留接口：未来应复制当前 PCB/地址空间/上下文。
   * 当前没有多进程地址空间，返回 -1 表示 ENOSYS。
   */
  return -1;
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
  /*
   * 预留接口：未来应分配内核栈、构造 Context，并把线程放入 runnable 队列。
   * 当前阶段不创建真实线程，保持单进程演示稳定。
   */
  (void)name;
  (void)entry;
  (void)arg;
  return -1;
}

void proc_thread_exit(int status) {
  proc_exit_current(status);
}

Context *proc_schedule(Context *prev) {
  if (current != NULL) current->cp = prev;
  return prev;
}

Context* schedule(Context *prev) {
  return proc_schedule(prev);
}
