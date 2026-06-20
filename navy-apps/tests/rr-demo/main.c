/*
 * rr-demo — Round-Robin scheduling demonstration
 * Uses SYS_yield for explicit context switching between parent and child.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2);
#define SYS_yield  1
#define SYS_fork   14
#define SYS_wait   17

int main() {
  printf("RR Demo: testing fork + yield\n");
  fflush(stdout);

  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    return 1;
  }

  if (pid == 0) {
    /* child */
    for (int i = 0; i < 40; i++) {
      printf("B");
      fflush(stdout);
      _syscall_(SYS_yield, 0, 0, 0);
    }
    printf(" child-exit\n");
    fflush(stdout);
    _exit(0);
  }

  /* parent */
  for (int i = 0; i < 40; i++) {
    printf("A");
    fflush(stdout);
    _syscall_(SYS_yield, 0, 0, 0);
  }
  printf(" parent-done\n");
  fflush(stdout);

  int status;
  wait(&status);
  printf("child status=%d\n", status);
  return 0;
}
