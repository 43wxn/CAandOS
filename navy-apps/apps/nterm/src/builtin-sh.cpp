#include <nterm.h>
#include <stdarg.h>
#include <unistd.h>
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/time.h>

#define SYS_shutdown 20

char handle_key(SDL_Event *ev);
extern "C" intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2);

static void sh_printf(const char *format, ...) {
  static char buf[512] = {};
  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  if (len > 0) {
    term->write(buf, len);
  }
}

static void sh_banner() {
  sh_printf("\033[2J\033[H");
  sh_printf("DTerm Server Shell on nanos-lite/riscv32-nemu\n");
  sh_printf("Type 'help' to list built-in commands.\n\n");
}

static void sh_prompt() {
  sh_printf("root@nanos-lite:/# ");
}

static const char *skip_space(const char *s) {
  while (*s == ' ' || *s == '\t') s++;
  return s;
}

static void make_path(const char *arg, char *out, size_t size) {
  if (arg == NULL || arg[0] == '\0') {
    snprintf(out, size, "/");
  } else if (arg[0] == '/') {
    snprintf(out, size, "%s", arg);
  } else {
    snprintf(out, size, "/%s", arg);
  }
}

static int parse_tokens(char *buf, char *argv[], int max_argc) {
  int argc = 0;
  char *tok = strtok(buf, " \t\r\n");
  while (tok != NULL && argc < max_argc - 1) {
    argv[argc++] = tok;
    tok = strtok(NULL, " \t\r\n");
  }
  argv[argc] = NULL;
  return argc;
}

static void cmd_help() {
  sh_printf("Built-ins:\n");
  sh_printf("  help              show this message\n");
  sh_printf("  clear             clear terminal\n");
  sh_printf("  pwd               print working directory\n");
  sh_printf("  ls [path]         list kernel-visible files\n");
  sh_printf("  cat <file>        print a file\n");
  sh_printf("  touch <file>      create an empty RAMFS file\n");
  sh_printf("  write <file> TXT  overwrite a RAMFS file\n");
  sh_printf("  append <file> TXT append to a RAMFS file\n");
  sh_printf("  rm <file>         remove a RAMFS file\n");
  sh_printf("  meminfo           show kernel memory/page usage\n");
  sh_printf("  ps                show the single-process model\n");
  sh_printf("  uptime            show timer syscall result\n");
  sh_printf("  shutdown          power off NEMU\n");
  sh_printf("  poweroff          same as shutdown\n");
  sh_printf("  run <program>     exec /bin/<program>\n");
  sh_printf("                    timer-test/hello/file-test print here\n");
}

static void cmd_cat_path(const char *arg) {
  char path[128];
  make_path(arg, path, sizeof(path));

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    sh_printf("cat: %s: no such file\n", path);
    return;
  }

  char buf[256];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    term->write(buf, n);
  }
  close(fd);
}

static bool file_exists(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return false;
  close(fd);
  return true;
}

static void list_proc_prefix(const char *dir) {
  int fd = open("/proc/files", O_RDONLY);
  if (fd < 0) return;

  char data[2048];
  int n = read(fd, data, sizeof(data) - 1);
  close(fd);
  if (n <= 0) return;
  data[n] = '\0';

  size_t dir_len = strlen(dir);
  char *line = strtok(data, "\n");
  while (line != NULL) {
    char type[16] = {};
    unsigned size = 0;
    char path[128] = {};
    if (sscanf(line, "%15s %u %127s", type, &size, path) == 3) {
      if (strcmp(dir, "/") == 0) {
        const char *name = path[0] == '/' ? path + 1 : path;
        if (strchr(name, '/') == NULL && strcmp(type, "ramfs") == 0) {
          sh_printf("%s  ", name);
        }
      } else if (strncmp(path, dir, dir_len) == 0 && path[dir_len] == '/') {
        const char *name = path + dir_len + 1;
        if (*name != '\0' && strchr(name, '/') == NULL) {
          sh_printf("%s  ", name);
        }
      }
    }
    line = strtok(NULL, "\n");
  }
}

static void cmd_ls(const char *arg) {
  char path[128];
  make_path(arg == NULL ? "/" : arg, path, sizeof(path));

  if (strcmp(path, "/") == 0) {
    sh_printf("bin/  dev/  proc/  share/  home/  etc/  ");
    list_proc_prefix("/");
    sh_printf("\n");
  } else if (strcmp(path, "/bin") == 0) {
    const char *bins[] = {
      "dterm", "bird", "pal", "hello", "timer-test",
      "file-test", "event-test", "dummy", NULL
    };
    for (int i = 0; bins[i] != NULL; i++) {
      char full[128];
      snprintf(full, sizeof(full), "/bin/%s", bins[i]);
      if (file_exists(full)) sh_printf("%s  ", bins[i]);
    }
    sh_printf("\n");
  } else if (strcmp(path, "/dev") == 0) {
    sh_printf("events  fb  sb  sbctl\n");
  } else if (strcmp(path, "/proc") == 0) {
    sh_printf("dispinfo  files  meminfo\n");
  } else {
    list_proc_prefix(path);
    sh_printf("\n");
  }
}

static void cmd_touch(const char *arg) {
  if (arg == NULL) {
    sh_printf("touch: missing file operand\n");
    return;
  }

  char path[128];
  make_path(arg, path, sizeof(path));
  int fd = open(path, O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    sh_printf("touch: cannot create %s\n", path);
    return;
  }
  close(fd);
}

static void cmd_write_text(const char *arg, bool append) {
  arg = skip_space(arg);
  if (*arg == '\0') {
    sh_printf("%s: missing file operand\n", append ? "append" : "write");
    return;
  }

  char name[128];
  int i = 0;
  while (*arg != '\0' && *arg != ' ' && *arg != '\t' && i < (int)sizeof(name) - 1) {
    name[i++] = *arg++;
  }
  name[i] = '\0';
  arg = skip_space(arg);

  char path[128];
  make_path(name, path, sizeof(path));

  int flags = O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC);
  int fd = open(path, flags, 0644);
  if (fd < 0) {
    sh_printf("%s: cannot open %s\n", append ? "append" : "write", path);
    return;
  }

  write(fd, arg, strlen(arg));
  write(fd, "\n", 1);
  close(fd);
}

static void cmd_rm(const char *arg) {
  if (arg == NULL) {
    sh_printf("rm: missing file operand\n");
    return;
  }

  char path[128];
  make_path(arg, path, sizeof(path));
  if (unlink(path) < 0) {
    sh_printf("rm: cannot remove %s\n", path);
  }
}

static void cmd_ps() {
  sh_printf("PID  PPID STATE    COMMAND\n");
  sh_printf("1    0    running  /bin/dterm\n");
  sh_printf("\nSingle-process demo: external programs replace dterm via execve,\n");
  sh_printf("and dterm is loaded again after SYS_exit.\n");
}

static void cmd_uptime() {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) == 0) {
    sh_printf("up %u.%06u seconds\n", (unsigned)tv.tv_sec, (unsigned)tv.tv_usec);
  } else {
    sh_printf("uptime: gettimeofday failed\n");
  }
}

static uint32_t now_ms() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static void wait_ms(uint32_t ms) {
  uint32_t start = now_ms();
  while (now_ms() - start < ms) {
    refresh_terminal();
  }
}

static bool cmd_run_builtin(const char *name) {
  if (strcmp(name, "hello") == 0 || strcmp(name, "/bin/hello") == 0) {
    sh_printf("Hello from /bin/hello (shown by the server shell).\n");
    return true;
  }

  if (strcmp(name, "timer-test") == 0 || strcmp(name, "/bin/timer-test") == 0) {
    sh_printf("timer-test: sampling gettimeofday()\n");
    for (int i = 0; i < 5; i++) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      sh_printf("  tick %d: %u.%06u s\n", i,
          (unsigned)tv.tv_sec, (unsigned)tv.tv_usec);
      refresh_terminal();
      wait_ms(500);
    }
    return true;
  }

  if (strcmp(name, "file-test") == 0 || strcmp(name, "/bin/file-test") == 0) {
    sh_printf("file-test: create, write, read, unlink /tmp-file\n");
    int fd = open("/tmp-file", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
      sh_printf("  open failed\n");
      return true;
    }
    const char *msg = "file-test payload\n";
    write(fd, msg, strlen(msg));
    lseek(fd, 0, SEEK_SET);
    char buf[64] = {};
    read(fd, buf, sizeof(buf) - 1);
    close(fd);
    sh_printf("  readback: %s", buf);
    unlink("/tmp-file");
    sh_printf("  done\n");
    return true;
  }

  return false;
}

static void cmd_run(const char *arg) {
  if (arg == NULL) {
    sh_printf("run: missing program name\n");
    return;
  }

  if (cmd_run_builtin(arg)) return;

  char path[128];
  if (arg[0] == '/') {
    snprintf(path, sizeof(path), "%s", arg);
  } else {
    snprintf(path, sizeof(path), "/bin/%s", arg);
  }

  char *exec_argv[] = { path, NULL };
  char *exec_envp[] = { NULL };
  execve(path, exec_argv, exec_envp);
  sh_printf("run: %s: no such program\n", path);
}

static void sh_handle_cmd(const char *cmd) {
  if (cmd == NULL) return;

  cmd = skip_space(cmd);
  if (*cmd == '\0') return;

  char buf[256];
  strncpy(buf, cmd, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *argv[8];
  int argc = parse_tokens(buf, argv, 8);
  if (argc == 0) return;

  const char *rest = cmd + strlen(argv[0]);

  if (strcmp(argv[0], "help") == 0) {
    cmd_help();
  } else if (strcmp(argv[0], "clear") == 0) {
    sh_printf("\033[2J\033[H");
  } else if (strcmp(argv[0], "pwd") == 0) {
    sh_printf("/\n");
  } else if (strcmp(argv[0], "ls") == 0) {
    cmd_ls(argc >= 2 ? argv[1] : "/");
  } else if (strcmp(argv[0], "cat") == 0) {
    if (argc < 2) sh_printf("cat: missing file operand\n");
    else cmd_cat_path(argv[1]);
  } else if (strcmp(argv[0], "touch") == 0) {
    cmd_touch(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "write") == 0) {
    cmd_write_text(rest, false);
  } else if (strcmp(argv[0], "append") == 0) {
    cmd_write_text(rest, true);
  } else if (strcmp(argv[0], "rm") == 0) {
    cmd_rm(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "meminfo") == 0) {
    cmd_cat_path("/proc/meminfo");
  } else if (strcmp(argv[0], "ps") == 0) {
    cmd_ps();
  } else if (strcmp(argv[0], "uptime") == 0) {
    cmd_uptime();
  } else if (strcmp(argv[0], "shutdown") == 0 || strcmp(argv[0], "poweroff") == 0) {
    sh_printf("Powering off NEMU...\n");
    refresh_terminal();
    _syscall_(SYS_shutdown, 0, 0, 0);
  } else if (strcmp(argv[0], "run") == 0 || strcmp(argv[0], "exec") == 0) {
    cmd_run(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "echo") == 0) {
    rest = skip_space(rest);
    sh_printf("%s\n", rest);
  } else if (strcmp(argv[0], "uname") == 0) {
    sh_printf("Nanos-lite riscv32-nemu single-process server demo\n");
  } else {
    sh_printf("%s: command not found\n", argv[0]);
  }

}

void builtin_sh_run() {
  sh_banner();
  sh_prompt();

  while (1) {
    SDL_Event ev;
    if (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_KEYUP || ev.type == SDL_KEYDOWN) {
        const char *res = term->keypress(handle_key(&ev));
        if (res) {
          sh_handle_cmd(res);
          sh_prompt();
        }
      }
    }
    refresh_terminal();
  }
}
