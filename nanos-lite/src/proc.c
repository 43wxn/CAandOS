#include <proc.h>

void naive_uload(PCB *pcb, const char *filename);

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb() {
  current = &pcb_boot;
}

void init_proc() {
  switch_boot_pcb();
  Log("Initializing processes...");

  // 每次只保留一个测试程序
  // naive_uload(NULL, "/bin/hello");
  // naive_uload(NULL, "/bin/timer-test");
  // naive_uload(NULL, "/bin/event-test");
  // naive_uload(NULL, "/bin/bmp-test");
  naive_uload(NULL, "/bin/bmp-test");
}

Context* schedule(Context *prev) {
  current->cp = prev;
  return prev;
}