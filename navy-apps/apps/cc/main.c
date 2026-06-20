/* cc — mini C compiler + VM for nanos-lite */
#include "cc.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/* VM output buffer (filled by codegen.c's vm_putc) */
extern char vm_out_buf[2048];
extern int  vm_out_pos;
extern void vm_out_reset(void);

static CC cc;
static VM vm;

/* Compiler output is collected here so dterm can display it after cc exits.
   We also echo everything to serial (host terminal) for debugging. */
#define CC_OUTPUT "/tmp/cc-out.txt"
static char out_buf[4096];
static int  out_pos;

static void out_add(const char *s) {
  size_t len = strlen(s);
  if (out_pos + (int)len >= (int)sizeof(out_buf) - 1)
    len = sizeof(out_buf) - 1 - out_pos;
  memcpy(out_buf + out_pos, s, len);
  out_pos += len;
  out_buf[out_pos] = '\0';
}

static void out_flush(void) {
  if (out_pos > 0) {
    int fd = open(CC_OUTPUT, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0) { write(fd, out_buf, out_pos); close(fd); }
    out_pos = 0;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("usage: cc <file.c> [-o <output>] [-run <file.out>]\n");
    return 1;
  }

  /* -run mode */
  if (strcmp(argv[1], "-run") == 0 && argc >= 3) {
    int fd = open(argv[2], O_RDONLY);
    if (fd < 0) { printf("cc: %s: no such file\n", argv[2]); return 1; }
    int n = read(fd, &vm, sizeof(vm));
    close(fd);
    if (n < (int)sizeof(vm)) { printf("cc: invalid bytecode\n"); return 1; }
    printf("cc: running %s...\n", argv[2]);
    out_add("--- output ---\n");
    vm_out_reset();
    int ret = vm_execute(&vm);
    if (vm_out_pos > 0) { vm_out_buf[vm_out_pos] = '\0'; out_add(vm_out_buf); }
    out_add("\n");
    out_flush();
    return ret;
  }

  /* compile mode */
  const char *src_file = argv[1];
  const char *out_file = NULL;
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0 && i+1 < argc) out_file = argv[++i];
  }

  /* read source */
  int fd = open(src_file, O_RDONLY);
  if (fd < 0) { printf("cc: %s: no such file\n", src_file); return 1; }
  int n = read(fd, cc.src, SRC_MAX - 1);
  close(fd);
  if (n <= 0) { printf("cc: %s: empty\n", src_file); return 1; }
  cc.src[n] = '\0';
  cc.src_len = n;

  {
    char msg[128];
    snprintf(msg, sizeof(msg), "cc: compiling %s (%d bytes)\n", src_file, n);
    printf("%s", msg); out_add(msg);
  }

  memset(&vm, 0, sizeof(vm));
  if (cc_compile(&cc, &vm) != 0) {
    printf("cc: compilation failed\n");
    out_flush();
    return 1;
  }

  {
    char msg[128];
    snprintf(msg, sizeof(msg), "cc: %d funcs, %d ops\n",
        vm.nfuncs, vm.code_len);
    printf("%s\n", msg); out_add(msg); out_add("\n");
  }

  /* save bytecode */
  if (out_file) {
    fd = open(out_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) { printf("cc: cannot create %s\n", out_file); out_flush(); return 1; }
    write(fd, &vm, sizeof(vm));
    close(fd);
    { char m[256]; snprintf(m, sizeof(m), "cc: wrote %s\n", out_file); printf("%s", m); out_add(m); }
  }

  /* run */
  if (!out_file) {
    out_add("--- output ---\n");
    printf("--- output ---\n");
    vm_out_reset();
    int ret = vm_execute(&vm);
    /* copy VM output to our buffer */
    if (vm_out_pos > 0) { vm_out_buf[vm_out_pos] = '\0'; out_add(vm_out_buf); }
    out_add("\n");
    printf("\n");
    { char m[64]; snprintf(m, sizeof(m), "--- exit %d ---\n", ret); printf("%s", m); out_add(m); }
    out_flush();
    return ret;
  }

  out_flush();
  return 0;
}
