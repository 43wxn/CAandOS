#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main(int argc, char *argv[], char *envp[]);

/*
 * Kernel writes argc/argv at 0x8FFF0000 before jumping to the entry point.
 * Format:
 *   +0: uint32_t argc
 *   +4: concatenated argv strings, each \0 terminated
 *
 * We reconstruct an argv[] array on the stack and pass it to main().
 */
#define USER_ARGS_ADDR  0x8FFF0000
#define USER_ARGC_MAX   32

extern char **environ;

void call_main(uintptr_t *args) {
  (void)args;

  uint32_t *base = (uint32_t *)USER_ARGS_ADDR;
  int argc = (int)*base;
  const char *strs = (const char *)(base + 1);

  /* Reconstruct argv on the stack */
  char *argv[USER_ARGC_MAX + 1];
  int ai = 0;

  if (argc > 0 && argc <= USER_ARGC_MAX) {
    const char *s = strs;
    for (int i = 0; i < argc && ai < USER_ARGC_MAX; i++) {
      argv[ai++] = (char *)s;
      s += strlen(s) + 1;  /* skip past the \0 */
    }
  }
  argv[ai] = NULL;

  char *empty[] = { NULL };
  environ = empty;
  exit(main(argc, argv, empty));
  assert(0);
}
