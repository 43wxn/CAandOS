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
  // 这里每次只加载一个测试程序
  // naive_uload(NULL, "/bin/file-test");
  // naive_uload(NULL, "/bin/dmp-test");
  // naive_uload(NULL, "/bin/event-test");
  naive_uload(NULL, "/bin/hello");
}

// PA3 单程序模型：直接返回当前上下文
Context* schedule(Context *prev) {
  current->cp = prev;
  return prev;
}