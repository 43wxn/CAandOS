#include <proc.h>

void naive_uload(PCB *pcb, const char *filename);

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb() {
  current = &pcb_boot;
}

void hello_fun(void *arg) {
  int j = 1;
  while (1) {
    Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (uintptr_t)arg, j);
    j ++;
    yield();
  }
}

void init_proc() {
  switch_boot_pcb();
  Log("Initializing processes...");
  // 加载用户程序
  naive_uload(NULL, "/bin/event-test");
}

// 修复：实现最简单的轮询调度
Context* schedule(Context *prev) {
  // 保存当前进程上下文
  current->cp = prev;

  // 轮询选择下一个就绪进程
  static int next_pcb = 0;
  next_pcb = (next_pcb + 1) % MAX_NR_PROC;

  // 切换到下一个进程
  current = &pcb[next_pcb];
  return current->cp;
}