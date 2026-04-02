#include <nterm.h>
#include <stdarg.h>
#include <unistd.h>
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char handle_key(SDL_Event *ev);

static void sh_printf(const char *format, ...) {
  static char buf[256] = {};
  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  if (len > 0) {
    term->write(buf, len);
  }
}

static void sh_banner() {
  sh_printf("Built-in Shell in NTerm (NJU Terminal)\n\n");
}

static void sh_prompt() {
  sh_printf("sh> ");
}

static void sh_handle_cmd(const char *cmd) {
  if (cmd == NULL) return;

  while (*cmd == ' ' || *cmd == '\t') cmd++;
  if (*cmd == '\0') return;

  // 先设置 PATH=/bin，这样输入 hello/pal/bird 就能找到 /bin/hello 等
  setenv("PATH", "/bin", 0);

  // 当前阶段先忽略参数，只取第一个 token
  char buf[256];
  strncpy(buf, cmd, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *argv[2];
  char *tok = strtok(buf, " \t\r\n");
  if (tok == NULL) return;

  argv[0] = tok;
  argv[1] = NULL;

  execvp(argv[0], argv);

  // 如果 execvp 返回，说明执行失败
  sh_printf("Command not found: %s\n", argv[0]);
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
