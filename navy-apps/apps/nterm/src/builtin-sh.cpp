#include <nterm.h>
#include <stdarg.h>
#include <unistd.h>
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/time.h>
#include <ctype.h>

#define SYS_shutdown 20
#define SYS_demo    21
#define PROC_FILES_BUFSIZE (8 * 1024)
#define HIST_MAX 50

char handle_key(SDL_Event *ev);
extern "C" intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2);

/* ========== ANSI colour macros ========== */
#define CLR_GREEN   "\033[1;32m"
#define CLR_RED     "\033[1;31m"
#define CLR_CYAN    "\033[1;36m"
#define CLR_BLUE    "\033[1;34m"
#define CLR_YELLOW  "\033[1;33m"
#define CLR_MAGENTA "\033[1;35m"
#define CLR_WHITE   "\033[1;37m"
#define CLR_RESET   "\033[0m"

/* ---- shared /proc/files buffer (replaces per-function static buffers) ---- */
static char proc_files_buf[PROC_FILES_BUFSIZE];

static int read_proc_files(char *buf, size_t bufsz) {
  int fd = open("/proc/files", O_RDONLY);
  if (fd < 0) return -1;
  int n = read(fd, buf, (int)(bufsz - 1));
  close(fd);
  if (n > 0) buf[n] = '\0';
  return n;
}

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

/* ========== standardised error printer: red prefix + message ========== */
static void sh_error(const char *prefix, const char *fmt, ...) {
  static char ebuf[512];
  int pos = snprintf(ebuf, sizeof(ebuf), CLR_RED "%s: " CLR_RESET, prefix);
  if (pos < (int)sizeof(ebuf)) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ebuf + pos, sizeof(ebuf) - (size_t)pos, fmt, ap);
    va_end(ap);
  }
  term->write(ebuf, strlen(ebuf));
}

/* ---- 多用户状态 ---- */
struct UserState {
  char work_dir[128];
  char *hist[HIST_MAX];
  uint32_t hist_time[HIST_MAX];
  int hist_cnt, hist_next;
  char input[256];
  int  inp_len;
  bool logged_in;
  char    term_buf[W * H];
  uint8_t term_color[W * H];
  int cursor_x, cursor_y;
};
static UserState users[3];
static int current_user = -1;
static const char *user_names[3] = {"User1", "User2", "User3"};

/* ========== motd 欢迎信息 ========== */
static void auto_login_from_file(void) {
  int fd = open("/tmp/.current_user", O_RDONLY);
  if (fd >= 0) {
    char buf[32]; int n = read(fd, buf, sizeof(buf)-1); close(fd);
    if (n > 0) { buf[n] = '\0';
      for (int i = 0; i < 3; i++) {
        if (strcmp(buf, user_names[i]) == 0) {
          current_user = i;
          users[i].logged_in = true;
          return;
        }
      }
    }
  }
}

static void save_current_user(void) {
  if (current_user >= 0) {
    int fd = open("/tmp/.current_user", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0) { write(fd, user_names[current_user], strlen(user_names[current_user])); close(fd); }
  }
}

static void sh_banner() {
  sh_printf("\033[2J");
  sh_printf("\033[H");

  /* 从文件恢复登录状态（execve 后 dterm 重启时用） */
  if (current_user < 0) auto_login_from_file();

  int outfd = open("/tmp/cc-out.txt", O_RDONLY);
  if (outfd >= 0) {
    char buf[2048]; ssize_t n = read(outfd, buf, sizeof(buf)-1);
    close(outfd); unlink("/tmp/cc-out.txt");
    if (n > 0) { buf[n] = '\0'; sh_printf("%s\n", buf); return; }
  }

  if (current_user < 0) {
    /* ---- Login banner ---- */
    sh_printf(CLR_BLUE);
    sh_printf("    +----------------------------------------+\n");
    sh_printf("    |        DLUT OS  -  nanos-lite          |\n");
    sh_printf("    |        Multi-User Terminal v1.0        |\n");
    sh_printf("    |                                        |\n");
    sh_printf("    |" CLR_RESET "   Users: " CLR_CYAN "User1" CLR_RESET "  " CLR_GREEN "User2" CLR_RESET "  " CLR_MAGENTA "User3" CLR_BLUE "   |\n");
    sh_printf("    |" CLR_RESET "   login <username> to start     " CLR_BLUE "|\n");
    sh_printf("    +----------------------------------------+\n");
    sh_printf(CLR_RESET "\n");
  } else {
    /* ---- Logged-in banner ---- */
    sh_printf(CLR_BLUE);
    sh_printf("    +----------------------------------------+\n");
    sh_printf("    |" CLR_RESET "        Welcome back, " CLR_GREEN "%-15s" CLR_BLUE "      |\n", user_names[current_user]);
    sh_printf("    +----------------------------------------+\n");
    sh_printf(CLR_RESET);
    int fd = open("/etc/motd", O_RDONLY);
    if (fd >= 0) {
      char buf[512]; ssize_t n = read(fd, buf, sizeof(buf)-1); close(fd);
      if (n > 0) { buf[n] = '\0'; sh_printf("%s", buf); }
    }
  }
  sh_printf("\n");
}

static void sh_prompt(void);

/* ---- 终端画面保存/恢复 ---- */
static void save_term_state(int u) {
  if (u < 0 || u >= 3) return;
  for (int i = 0; i < W * H; i++) {
    users[u].term_buf[i]   = term->getch(i % W, i / W);
    users[u].term_color[i] = (term->foreground(i % W, i / W) & 0xFF);
  }
  users[u].cursor_x = term->cursor.x;
  users[u].cursor_y = term->cursor.y;
}
static void restore_term_state(int u) {
  if (u < 0 || u >= 3) return;
  for (int y = 3; y < H; y++)
    for (int x = 0; x < W; x++) {
      char ch = users[u].term_buf[y * W + x];
      term->putch(x, y, ch ? ch : ' ');
    }
  term->cursor.x = users[u].cursor_x >= 0 ? users[u].cursor_x : 0;
  term->cursor.y = users[u].cursor_y >= 3 ? users[u].cursor_y : 3;
}
#define cwd (current_user >= 0 && users[current_user].work_dir[0] ? users[current_user].work_dir : (char *)"/")

/* ========== 彩色提示符 ========== */
static void sh_prompt() {
  if (current_user < 0) {
    sh_printf(CLR_GREEN "login> " CLR_RESET);
  } else {
    sh_printf(CLR_GREEN "%s@nanos-lite" CLR_RESET ":" CLR_CYAN "%s" CLR_RESET "# ",
        user_names[current_user], cwd);
  }
}

static const char *skip_space(const char *s) {
  while (*s == ' ' || *s == '\t') s++;
  return s;
}

static void cmd_cd(const char *arg) {
  if (arg == NULL || arg[0] == '\0' || strcmp(arg, "/") == 0) {
    strcpy(cwd, "/"); return;
  }
  if (strcmp(arg, "..") == 0) {
    if (strcmp(cwd, "/") == 0) return;
    char *last = strrchr(cwd, '/');
    if (last == cwd) strcpy(cwd, "/");
    else if (last) *last = '\0';
    return;
  }
  if (strcmp(arg, ".") == 0) return;
  char tmp[256];
  if (arg[0] == '/') snprintf(tmp, sizeof(tmp), "%s", arg);
  else if (strcmp(cwd, "/") == 0) snprintf(tmp, sizeof(tmp), "/%s", arg);
  else snprintf(tmp, sizeof(tmp), "%s/%s", cwd, arg);
  /* 验证目录存在 */
  bool exists = false;
  int n = read_proc_files(proc_files_buf, sizeof(proc_files_buf));
  if (n > 0) {
      size_t tlen = strlen(tmp);
      char *line = strtok(proc_files_buf, "\n");
      while (line) {
        char p[128];
        if (sscanf(line, "%*s %*u %127s", p) == 1) {
          if (strncmp(p, tmp, tlen) == 0 && (p[tlen] == '/' || p[tlen] == '\0'))
            { exists = true; break; }
        }
        line = strtok(NULL, "\n");
      }
    }
  const char *known[] = {"/bin","/dev","/proc","/share","/home","/etc","/test","/demos",NULL};
  for (int i = 0; known[i]; i++) if (strcmp(tmp, known[i]) == 0) exists = true;
  if (exists) strncpy(cwd, tmp, sizeof(cwd) - 1);
  else sh_error("cd", "%s: no such directory\n", tmp);
}

static void make_path(const char *arg, char *out, size_t size) {
  if (arg == NULL || arg[0] == '\0') snprintf(out, size, "%s", cwd);
  else if (arg[0] == '/') snprintf(out, size, "%s", arg);
  else if (strcmp(cwd, "/") == 0) snprintf(out, size, "/%s", arg);
  else snprintf(out, size, "%s/%s", cwd, arg);
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

/* 前向声明，解决 cross-reference */
static uint32_t now_ms();
static void sh_handle_cmd(const char *cmd);

/* ========== history 命令历史（增强版：带时间戳 + redo 命令） ========== */
static char *hist[HIST_MAX] = {};
static uint32_t hist_time[HIST_MAX] = {};
static int hist_cnt = 0;
static int hist_next = 0;

static void hist_add(const char *cmd) {
  if (cmd == NULL || cmd[0] == '\0') return;
  free(hist[hist_next]);
  hist[hist_next] = strdup(cmd);
  hist_time[hist_next] = now_ms();
  hist_next = (hist_next + 1) % HIST_MAX;
  if (hist_cnt < HIST_MAX) hist_cnt++;
}

static void cmd_history() {
  for (int i = 0; i < hist_cnt; i++) {
    int idx = (hist_next - hist_cnt + i + HIST_MAX) % HIST_MAX;
    uint32_t elapsed = hist_time[idx] - hist_time[(hist_next - hist_cnt + HIST_MAX) % HIST_MAX];
    uint32_t sec = elapsed / 1000;
    uint32_t ms = elapsed % 1000;
    sh_printf(" %4d  [%3u.%03us]  %s\n", i + 1, sec, ms, hist[idx]);
  }
}

static const char *hist_get(int n) {
  if (n < 1 || n > hist_cnt) return NULL;
  int idx = (hist_next - hist_cnt + n - 1 + HIST_MAX) % HIST_MAX;
  return hist[idx];
}

/* redo N — 重新执行历史命令（替代 !N） */
static void cmd_redo(int argc, char *argv[]) {
  if (argc < 2) {
    sh_error("redo", "missing history number (use 'history' to list)\n");
    return;
  }
  int n = atoi(argv[1]);
  const char *replay = hist_get(n);
  if (replay) {
    sh_printf("!%d: %s\n", n, replay);
    sh_handle_cmd(replay);
  } else {
    sh_error("redo", "no such command: %d\n", n);
  }
}

/* ========== date 模拟日期 ========== */
static long date_offset = 0; // seconds offset from real uptime

static void cmd_date(int argc, char *argv[]) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  long abs_sec = (long)tv.tv_sec + date_offset;

  if (argc >= 3 && strcmp(argv[1], "-s") == 0) {
    int y = 0, m = 0, d = 0, hh = 0, mm = 0, ss = 0;
    int nd = sscanf(argv[2], "%d-%d-%d", &y, &m, &d);
    if (nd != 3) {
      sh_error("date", "invalid date format. Use: date -s YYYY-MM-DD [HH:MM:SS]\n");
      return;
    }
    if (argc >= 4) sscanf(argv[3], "%d:%d:%d", &hh, &mm, &ss);

    long target = 0;
    int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for (int yr = 2025; yr < y; yr++) target += 365L * 86400;
    for (int mo = 1; mo < m && mo <= 12; mo++) target += dim[mo - 1] * 86400L;
    if (d > 0) target += (d - 1) * 86400L;
    target += hh * 3600L + mm * 60L + ss;
    date_offset = target - (long)tv.tv_sec;
    sh_printf("date: set to ");
  }

  long days = abs_sec / 86400;
  long rem = abs_sec % 86400;
  int h = (int)(rem / 3600);
  int mi = (int)((rem % 3600) / 60);
  int s = (int)(rem % 60);

  int yr = 2025;
  int mo = 1;
  long dleft = days;
  int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  while (dleft >= 365) { yr++; dleft -= 365; }
  for (int i = 0; i < 12 && dleft >= dim[i]; i++) {
    dleft -= dim[i];
    mo++;
  }
  int dy = (int)dleft + 1;

  sh_printf("%04d-%02d-%02d %02d:%02d:%02d\n", yr, mo, dy, h, mi, s);
}

/* ========== hexdump ========== */
static void cmd_hexdump(int argc, char *argv[]) {
  if (argc < 2) {
    sh_printf("usage: hexdump [-n MAXBYTES] <file>\n");
    return;
  }
  char path[128];
  int max_bytes = 512;
  int file_idx = 1;

  if (argc >= 3 && strcmp(argv[1], "-n") == 0) {
    max_bytes = atoi(argv[2]);
    file_idx = 3;
  }
  if (file_idx >= argc) {
    sh_error("hexdump", "missing file operand\n");
    return;
  }
  make_path(argv[file_idx], path, sizeof(path));

  int fd = open(path, O_RDONLY);
  if (fd < 0) { sh_error("hexdump", "%s: no such file\n", path); return; }

  unsigned char buf[16];
  int offset = 0, n;
  while (offset < max_bytes && (n = read(fd, buf, sizeof(buf))) > 0) {
    sh_printf("%08x  ", offset);
    // hex part
    for (int i = 0; i < 16; i++) {
      if (i < n) sh_printf("%02x ", buf[i]);
      else sh_printf("   ");
      if (i == 7) sh_printf(" ");
    }
    sh_printf(" |");
    // ascii part
    for (int i = 0; i < n; i++)
      sh_printf("%c", isprint(buf[i]) ? buf[i] : '.');
    sh_printf("|\n");
    offset += n;
  }
  close(fd);
}

/* ========== free 命令（增强：可视化内存条） ========== */
static void cmd_free() {
  int fd = open("/proc/meminfo", O_RDONLY);
  if (fd < 0) {
    sh_error("free", "cannot read /proc/meminfo\n");
    return;
  }
  static char buf[1024];
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return;
  buf[n] = '\0';

  unsigned total_kb = 0, used_kb = 0, free_kb = 0, pagesz = 0;
  char *line = strtok(buf, "\n");
  while (line) {
    unsigned val;
    if (sscanf(line, "MemTotal: %u kB", &val) == 1) total_kb = val;
    if (sscanf(line, "MemUsed: %u kB", &val) == 1) used_kb = val;
    if (sscanf(line, "MemFree: %u kB", &val) == 1) free_kb = val;
    if (sscanf(line, "PageSize: %u", &val) == 1) pagesz = val;
    line = strtok(NULL, "\n");
  }

  /* visual memory bar */
  int bar_width = 30;
  int used_chars = (total_kb > 0) ? (int)((unsigned long long)used_kb * bar_width / total_kb) : 0;
  if (used_chars > bar_width) used_chars = bar_width;
  int pct = (total_kb > 0) ? (int)((unsigned long long)used_kb * 100 / total_kb) : 0;

  sh_printf(CLR_CYAN "  Memory:" CLR_RESET " [");
  for (int i = 0; i < bar_width; i++) {
    if (i < used_chars) sh_printf(CLR_GREEN "#" CLR_RESET);
    else sh_printf(".");
  }
  sh_printf("] %s%d%%%s\n",
      pct > 75 ? CLR_RED : (pct > 50 ? CLR_YELLOW : CLR_GREEN), pct, CLR_RESET);

  /* summary line */
  sh_printf("  %uK" CLR_RESET " / %uK total\n", used_kb, total_kb);

  /* number table */
  sh_printf("             %s%10s%10s%10s%s\n",
      CLR_CYAN, "total", "used", "free", CLR_RESET);
  sh_printf("  Mem:    %10u %10u %10u\n", total_kb, used_kb, free_kb);
  if (pagesz) sh_printf("  Page size: %u bytes\n", pagesz);
}

/* ---- wc 命令 ---- */
static void cmd_wc(int argc, char *argv[]) {
  if (argc < 2) { sh_error("wc", "missing file operand\n"); return; }
  char path[128];
  make_path(argv[argc - 1], path, sizeof(path));

  int fd = open(path, O_RDONLY);
  if (fd < 0) { sh_error("wc", "%s: no such file\n", path); return; }

  char buf[512];
  int lines = 0, words = 0, bytes = 0;
  bool in_word = false;
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (ssize_t i = 0; i < n; i++) {
      bytes++;
      if (buf[i] == '\n') lines++;
      if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r') {
        in_word = false;
      } else if (!in_word) {
        in_word = true;
        words++;
      }
    }
  }
  close(fd);

  bool flag_l = false, flag_w = false, flag_c = false;
  for (int i = 1; i < argc - 1; i++) {
    if (strchr(argv[i], 'l')) flag_l = true;
    if (strchr(argv[i], 'w')) flag_w = true;
    if (strchr(argv[i], 'c')) flag_c = true;
  }
  if (!flag_l && !flag_w && !flag_c) flag_l = flag_w = flag_c = true;

  if (flag_l) sh_printf(" %d", lines);
  if (flag_w) sh_printf(" %d", words);
  if (flag_c) sh_printf(" %d", bytes);
  sh_printf(" %s\n", path);
}

/* ========== env 命令 ========== */
static void cmd_env() {
  extern char **environ;
  if (environ) {
    for (int i = 0; environ[i]; i++)
      sh_printf("%s\n", environ[i]);
  }
}

/* ========== yes 命令 ========== */
static void cmd_yes(const char *arg) {
  const char *s = arg ? arg : "y";
  while (1) {
    sh_printf("%s\n", s);
    refresh_terminal();
  }
}

/* ========== help — 分类彩色显示 ========== */

struct HelpEntry { const char *cmd; const char *desc; };
struct HelpCategory { const char *name; const HelpEntry *entries; int count; };

static const HelpEntry help_files[] = {
  {"cat <file>",       "print a file"},
  {"touch <file>",     "create an empty RAMFS file"},
  {"write <f> TXT",    "overwrite a RAMFS file"},
  {"append <f> TXT",   "append to a RAMFS file"},
  {"rm <file>",        "remove a RAMFS file"},
};

static const HelpEntry help_dirs[] = {
  {"cd <dir>",         "change working directory"},
  {"mkdir <dir>",      "create a directory"},
  {"rmdir <dir>",      "remove a directory"},
  {"pwd",              "print working directory"},
  {"ls [path]",        "list files"},
  {"tree [path]",      "show directory tree"},
};

static const HelpEntry help_info[] = {
  {"ps",               "show process table"},
  {"free",             "show memory summary"},
  {"uptime",           "show system uptime"},
  {"meminfo",          "show kernel memory info"},
  {"date [-s ...]",    "show/set simulated date"},
  {"uname",            "print system name"},
  {"hexdump [-n N] F", "hex+ASCII dump"},
  {"wc [-lwc] <file>", "count lines/words/bytes"},
};

static const HelpEntry help_shell[] = {
  {"help",             "show this message"},
  {"clear",            "clear terminal"},
  {"history",          "show command history"},
  {"redo <N>",         "re-execute history entry N"},
  {"echo <text>",      "print text"},
  {"env",              "show environment variables"},
  {"yes [str]",        "repeat output"},
};

static const HelpEntry help_progs[] = {
  {"run <prog> [args]","exec /bin/<prog> with args"},
  {"cc <file.c>",      "compile & run C source"},
  {"chat",             "AI therapist chatbot"},
  {"shutdown",         "power off NEMU"},
};

static const HelpCategory help_cats[] = {
  {"File Operations", help_files, 5},
  {"Directory  Ops",  help_dirs,  6},
  {"System  Info",    help_info,  8},
  {"Shell  Utils",    help_shell, 7},
  {"Programs",        help_progs, 4},
};

static void cmd_help() {
  for (int c = 0; c < 5; c++) {
    sh_printf(CLR_CYAN "  --- %s ---" CLR_RESET "\n", help_cats[c].name);
    for (int e = 0; e < help_cats[c].count; e++) {
      sh_printf("  " CLR_GREEN "%-17s" CLR_RESET " %s\n",
          help_cats[c].entries[e].cmd,
          help_cats[c].entries[e].desc);
    }
    sh_printf("\n");
  }
}

static void cmd_cat_path(const char *arg) {
  char path[128];
  make_path(arg, path, sizeof(path));

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    sh_error("cat", "%s: no such file\n", path);
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

/* ========== LS 多列布局 ========== */
#define LS_MAX_ENTRIES 64

struct LsEntry {
  char name[32];
  bool is_dir;
};

/* Print entries in N columns (column-major), directories coloured CYAN */
static void print_ls_columns(LsEntry *entries, int count) {
  if (count == 0) { sh_printf("  (empty)\n"); return; }

  int max_len = 0;
  for (int i = 0; i < count; i++) {
    int len = (int)strlen(entries[i].name);
    if (len > max_len) max_len = len;
  }

  int col_width = max_len + 2;
  int cols = (W - 2) / col_width;
  if (cols < 1) cols = 1;
  if (cols > count) cols = count;
  int rows = (count + cols - 1) / cols;

  for (int r = 0; r < rows; r++) {
    sh_printf("  ");
    for (int c = 0; c < cols; c++) {
      int idx = c * rows + r;  /* column-major */
      if (idx >= count) break;
      LsEntry *e = &entries[idx];
      if (e->is_dir) sh_printf(CLR_CYAN "%s" CLR_RESET, e->name);
      else           sh_printf("%s", e->name);
      int pad = col_width - (int)strlen(e->name);
      for (int p = 0; p < pad; p++) sh_printf(" ");
    }
    sh_printf("\n");
  }
}

/* Collect direct children of `dir` from /proc/files */
static void collect_dir_entries(const char *dir, LsEntry *entries, int *count) {
  int n = read_proc_files(proc_files_buf, sizeof(proc_files_buf));
  if (n <= 0) return;

  size_t dir_len = strlen(dir);
  char *line = strtok(proc_files_buf, "\n");
  while (line && *count < LS_MAX_ENTRIES) {
    char type[16] = {};
    char path[128] = {};
    if (sscanf(line, "%15s %*u %127s", type, path) >= 2) {
      if (strncmp(path, dir, dir_len) == 0 && path[dir_len] == '/') {
        const char *name = path + dir_len + 1;
        if (*name != '\0' && strchr(name, '/') == NULL) {
          strncpy(entries[*count].name, name, 31);
          entries[*count].name[31] = '\0';
          entries[*count].is_dir = (strcmp(type, "ramfs-dir") == 0 || strcmp(type, "dyn-dir") == 0);
          (*count)++;
        }
      }
    }
    line = strtok(NULL, "\n");
  }
}

static void cmd_ls(const char *arg) {
  char path[128];
  make_path(arg ? arg : cwd, path, sizeof(path));

  LsEntry entries[LS_MAX_ENTRIES];
  int count = 0;

  if (strcmp(path, "/") == 0) {
    /* Known top-level directories */
    const char *known_dirs[] = {"bin","dev","proc","share","home","etc","test","demos",NULL};
    for (int i = 0; known_dirs[i] && count < LS_MAX_ENTRIES; i++) {
      strncpy(entries[count].name, known_dirs[i], 31);
      entries[count].is_dir = true;
      count++;
    }
    /* Dynamic root-level files from /proc/files */
    int n = read_proc_files(proc_files_buf, sizeof(proc_files_buf));
    if (n > 0) {
      char *saveptr;
      char *line = strtok_r(proc_files_buf, "\n", &saveptr);
      while (line && count < LS_MAX_ENTRIES) {
        char *last_space = strrchr(line, ' ');
        if (last_space) {
          const char *p = last_space + 1;
          while (*p == ' ') p++;
          if (p[0] == '/' && strchr(p + 1, '/') == NULL) {
            strncpy(entries[count].name, p + 1, 31);
            entries[count].is_dir = false;
            count++;
          }
        }
        line = strtok_r(NULL, "\n", &saveptr);
      }
    }
  } else if (strcmp(path, "/bin") == 0) {
    const char *bins[] = {
      "dterm", "bird", "pal", "hello", "timer-test",
      "file-test", "event-test", "dummy", NULL
    };
    for (int i = 0; bins[i] && count < LS_MAX_ENTRIES; i++) {
      char full[128];
      snprintf(full, sizeof(full), "/bin/%s", bins[i]);
      if (file_exists(full)) {
        strncpy(entries[count].name, bins[i], 31);
        entries[count].is_dir = false;
        count++;
      }
    }
  } else if (strcmp(path, "/dev") == 0) {
    const char *devs[] = {"events","fb","sb","sbctl",NULL};
    for (int i = 0; devs[i] && count < LS_MAX_ENTRIES; i++) {
      strncpy(entries[count].name, devs[i], 31);
      entries[count].is_dir = false;
      count++;
    }
    collect_dir_entries("/dev", entries, &count);
  } else if (strcmp(path, "/proc") == 0) {
    const char *procs[] = {"dispinfo","files","meminfo","processes",NULL};
    for (int i = 0; procs[i] && count < LS_MAX_ENTRIES; i++) {
      strncpy(entries[count].name, procs[i], 31);
      entries[count].is_dir = false;
      count++;
    }
    collect_dir_entries("/proc", entries, &count);
  } else if (strcmp(path, "/home") == 0) {
    int n = read_proc_files(proc_files_buf, sizeof(proc_files_buf));
    if (n > 0) {
      char *line = strtok(proc_files_buf, "\n");
      while (line && count < LS_MAX_ENTRIES) {
        char p[128];
        if (sscanf(line, "%*s %*u %127s", p) >= 1) {
          if (strncmp(p, "/home/", 6) == 0 && strchr(p + 6, '/') == NULL) {
            strncpy(entries[count].name, p + 6, 31);
            entries[count].is_dir = true;
            count++;
          }
        }
        line = strtok(NULL, "\n");
      }
    }
  } else {
    collect_dir_entries(path, entries, &count);
  }

  print_ls_columns(entries, count);
}

/* ---- tree 命令 ---- */
static int path_cmp(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

static void cmd_tree(const char *arg) {
  char root[128];
  make_path(arg ? arg : "/", root, sizeof(root));

  int n = read_proc_files(proc_files_buf, sizeof(proc_files_buf));
  if (n <= 0) { sh_printf("%s\n(empty)\n", root); return; }

  sh_printf(CLR_CYAN "%s" CLR_RESET "\n", root);

  // 收集所有路径
  static const char *paths[512];
  int pcnt = 0;
  if (n > 0) {
    char *line = strtok(proc_files_buf, "\n");
    while (line && pcnt < 512) {
      char type[16], p[128];
      if (sscanf(line, "%15s %*u %127s", type, p) == 2) {
        size_t rlen = strlen(root);
        if (strncmp(p, root, rlen) == 0) {
          char *cpy = strdup(p);
          if (cpy) paths[pcnt++] = cpy;
        }
      }
      line = strtok(NULL, "\n");
    }
  }
  // 添加硬编码的已知目录
  const char *known[] = {
    "/bin/dterm","/bin/bird","/bin/pal","/bin/hello","/bin/timer-test",
    "/bin/file-test","/bin/event-test","/bin/dummy",
    "/dev/events","/dev/fb","/dev/sb","/dev/sbctl",
    "/proc/dispinfo","/proc/files","/proc/meminfo","/proc/processes",
    "/home/welcome.txt",
    NULL
  };
  size_t rlen = strlen(root);
  for (int i = 0; known[i] && pcnt < 480; i++) {
    if (strncmp(known[i], root, rlen) == 0) {
      bool dup = false;
      for (int j = 0; j < pcnt; j++) {
        if (strcmp(paths[j], known[i]) == 0) { dup = true; break; }
      }
      if (!dup) paths[pcnt++] = strdup(known[i]);
    }
  }
  qsort(paths, pcnt, sizeof(char *), path_cmp);

  // 缩进打印
  for (int i = 0; i < pcnt; i++) {
    const char *p = paths[i] + rlen;
    if (*p == '/') p++;
    if (*p == '\0') { free((void *)paths[i]); continue; }

    // 解析路径层级
    char parts[8][64] = {};
    int depth = 0;
    const char *s = p;
    while (*s && depth < 8) {
      const char *slash = strchr(s, '/');
      if (slash) {
        size_t len = slash - s;
        if (len >= 64) len = 63;
        memcpy(parts[depth], s, len);
        parts[depth][len] = '\0';
        depth++;
        s = slash + 1;
      } else {
        strncpy(parts[depth], s, 63);
        parts[depth][63] = '\0';
        depth++;
        break;
      }
    }

    // 计算缩进和连接线
    bool is_last[8] = {};
    for (int d = 0; d < depth; d++) {
      is_last[d] = true;
      for (int j = i + 1; j < pcnt; j++) {
        const char *jp = paths[j] + rlen;
        if (*jp == '/') jp++;
        char jparts[8][64] = {};
        int jd = 0;
        const char *js = jp;
        while (*js && jd < 8) {
          const char *sl = strchr(js, '/');
          if (sl) {
            size_t l = sl - js;
            if (l >= 64) l = 63;
            memcpy(jparts[jd], js, l);
            jparts[jd][l] = '\0';
            jd++;
            js = sl + 1;
          } else {
            strncpy(jparts[jd], js, 63);
            jparts[jd][63] = '\0';
            jd++;
            break;
          }
        }
        if (jd > d && strcmp(jparts[d], parts[d]) == 0) {
          is_last[d] = false;
          break;
        }
        if (jd <= d || strcmp(jparts[d], parts[d]) != 0) break;
      }
    }

    // 打印缩进
    for (int d = 0; d < depth - 1; d++) {
      if (is_last[d]) sh_printf("    ");
      else sh_printf("|   ");
    }
    if (depth > 0) {
      sh_printf(is_last[depth - 1] ? "`-- " : "|-- ");
    }

    // 判断是目录还是文件
    bool is_dir = false;
    for (int j = i + 1; j < pcnt; j++) {
      const char *jp = paths[j] + rlen;
      if (*jp == '/') jp++;
      if (strncmp(jp, p, strlen(p)) == 0 && jp[strlen(p)] == '/') {
        is_dir = true;
        break;
      }
    }
    sh_printf("%s%s" CLR_RESET "\n", is_dir ? CLR_CYAN : "", parts[depth - 1]);

    free((void *)paths[i]);
  }
}

static void cmd_touch(const char *arg) {
  if (arg == NULL) {
    sh_error("touch", "missing file operand\n");
    return;
  }

  char path[128];
  make_path(arg, path, sizeof(path));
  int fd = open(path, O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    sh_error("touch", "cannot create %s\n", path);
    return;
  }
  close(fd);
}

static void cmd_write_text(const char *arg, bool append) {
  arg = skip_space(arg);
  const char *pname = append ? "append" : "write";
  if (*arg == '\0') {
    sh_error(pname, "missing file operand\n");
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
    sh_error(pname, "cannot open %s\n", path);
    return;
  }

  write(fd, arg, strlen(arg));
  write(fd, "\n", 1);
  close(fd);
}

static void cmd_rm(const char *arg) {
  if (arg == NULL) {
    sh_error("rm", "missing file operand\n");
    return;
  }

  char path[128];
  make_path(arg, path, sizeof(path));
  if (unlink(path) < 0) {
    sh_error("rm", "cannot remove %s\n", path);
  }
}

static void cmd_mkdir(const char *arg) {
  if (arg == NULL || *arg == '\0') {
    sh_error("mkdir", "missing operand\n");
    return;
  }
  char path[128];
  make_path(arg, path, sizeof(path));
  int fd = open(path, O_CREAT | O_RDWR, 0755);
  if (fd < 0) {
    sh_error("mkdir", "cannot create directory %s\n", path);
    return;
  }
  close(fd);
}

static void cmd_rmdir(const char *arg) {
  if (arg == NULL || *arg == '\0') {
    sh_error("rmdir", "missing operand\n");
    return;
  }
  char path[128];
  make_path(arg, path, sizeof(path));
  if (unlink(path) < 0) {
    sh_error("rmdir", "failed to remove %s\n", path);
  }
}

/* ========== PS — 彩色进程表 ========== */
static void cmd_ps() {
  int fd = open("/proc/processes", O_RDONLY);
  if (fd < 0) {
    // fallback
    sh_printf(CLR_CYAN "  PID  PPID  STATE      COMMAND" CLR_RESET "\n");
    sh_printf("  1    0    " CLR_GREEN "running" CLR_RESET "  /bin/dterm\n");
    return;
  }
  char buf[1024];
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return;
  buf[n] = '\0';

  /* Coloured header */
  sh_printf(CLR_CYAN "  PID  PPID  STATE      COMMAND" CLR_RESET "\n");
  sh_printf(CLR_CYAN "  ---- ----  ---------  --------------------" CLR_RESET "\n");

  char *line = strtok(buf, "\n");
  bool first = true;
  while (line) {
    if (first) { first = false; line = strtok(NULL, "\n"); continue; }
    int pid, ppid;
    char state[16] = {}, cmd[64] = {};
    if (sscanf(line, "%d %d %15s %63s", &pid, &ppid, state, cmd) >= 3) {
      const char *sc = CLR_RESET;
      if (strcmp(state, "running") == 0)      sc = CLR_GREEN;
      else if (strcmp(state, "runnable") == 0) sc = CLR_YELLOW;
      else if (strcmp(state, "zombie") == 0)   sc = CLR_RED;

      sh_printf("  %4d  %4d  %s%-9s" CLR_RESET "  %s\n", pid, ppid, sc, state, cmd);
    }
    line = strtok(NULL, "\n");
  }
}

static void cmd_uptime() {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    sh_error("uptime", "gettimeofday failed\n");
    return;
  }
  unsigned s = (unsigned)tv.tv_sec;
  unsigned days = s / 86400;
  s %= 86400;
  unsigned hours = s / 3600;
  s %= 3600;
  unsigned mins = s / 60;
  unsigned secs = s % 60;

  sh_printf("  " CLR_CYAN "Uptime:" CLR_RESET " ");
  if (days > 0) sh_printf("%u day%s, ", days, days == 1 ? "" : "s");
  if (hours > 0 || days > 0) sh_printf("%u hour%s, ", hours, hours == 1 ? "" : "s");
  if (mins > 0 || hours > 0 || days > 0) sh_printf("%u min%s, ", mins, mins == 1 ? "" : "s");
  sh_printf("%u sec%s\n", secs, secs == 1 ? "" : "s");
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

/*
 * cmd_run — 通过 execve 加载运行 /bin/ 下的 ELF 程序。
 */
static void cmd_run(int argc, char *argv[]) {
  if (argc < 1 || argv[0] == NULL) {
    sh_error("run", "missing program name\n");
    return;
  }

  const char *prog = argv[0];
  char path[128];
  if (prog[0] == '/') {
    snprintf(path, sizeof(path), "%s", prog);
  } else {
    snprintf(path, sizeof(path), "/bin/%s", prog);
  }

  char *exec_argv[16];
  int eargc = 0;
  exec_argv[eargc++] = path;
  for (int i = 1; i < argc && eargc < 15; i++) {
    exec_argv[eargc++] = argv[i];
  }
  exec_argv[eargc] = NULL;
  char *exec_envp[] = { NULL };

  save_current_user();
  refresh_terminal();
  execve(path, exec_argv, exec_envp);

  /* ---- execve 失败才走到这里 ---- */

  if (strcmp(prog, "hello") == 0 || strcmp(prog, "/bin/hello") == 0) {
    sh_printf("Hello from /bin/hello (built-in fallback).\n");
    return;
  }

  if (strcmp(prog, "timer-test") == 0 || strcmp(prog, "/bin/timer-test") == 0) {
    sh_printf("timer-test: sampling gettimeofday()\n");
    for (int i = 0; i < 5; i++) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      sh_printf("  tick %d: %u.%06u s\n", i,
          (unsigned)tv.tv_sec, (unsigned)tv.tv_usec);
      refresh_terminal();
      wait_ms(500);
    }
    return;
  }

  if (strcmp(prog, "file-test") == 0 || strcmp(prog, "/bin/file-test") == 0) {
    sh_printf("file-test: create, write, read, unlink /tmp-file\n");
    int fd = open("/tmp-file", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
      sh_printf("  open failed\n");
      return;
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
    return;
  }

  if (strcmp(prog, "yes") == 0 || strcmp(prog, "/bin/yes") == 0) {
    cmd_yes(argc >= 2 ? argv[1] : NULL);
    return;
  }

  sh_error("run", "%s: no such program\n", path);
}

/*
 * cmd_chat — ELIZA 风格的 AI 对话程序
 */
static void cmd_chat(void) {
  sh_printf("\033[2J\033[H");
  sh_printf(CLR_CYAN " Nano-AI Therapist v1.0" CLR_RESET "\n");
  sh_printf(" Based on ELIZA (Weizenbaum, MIT 1966)\n");
  sh_printf(" Type 'quit' to exit.\n\n");

  static const char *greetings[] = {
    "Hello. How are you feeling today?",
    "Welcome. What brings you here?",
    "Hi. Tell me what's on your mind.",
    "Good day. How can I help you?",
  };
  sh_printf(CLR_GREEN "AI" CLR_RESET ": %s\n",
      greetings[now_ms() % 4]);

  char input[256];
  int turns = 0;

  while (turns < 50) {
    sh_printf(CLR_CYAN "You" CLR_RESET ": ");
    refresh_terminal();

    int pos = 0;
    while (pos < (int)sizeof(input) - 1) {
      SDL_Event ev;
      while (!SDL_PollEvent(&ev)) refresh_terminal();
      char ch = handle_key(&ev);
      if (ch == '\0') continue;
      if (ch == '\n') { sh_printf("\n"); break; }
      if (ch == '\b') {
        if (pos > 0) {
          pos--;
          if (term->cursor.x > 0) term->cursor.x--;
          else if (term->cursor.y > 0) { term->cursor.y--; term->cursor.x = term->w - 1; }
          term->putch(term->cursor.x, term->cursor.y, ' ');
        }
        continue;
      }
      if (ch >= ' ') {
        input[pos++] = ch;
        sh_printf("%c", ch); refresh_terminal();
      }
    }
    input[pos] = '\0';
    turns++;

    while (pos > 0 && (input[pos-1] == ' ' || input[pos-1] == '\t'))
      input[--pos] = '\0';

    if (strcmp(input, "quit") == 0 || strcmp(input, "bye") == 0 ||
        strcmp(input, "exit") == 0) {
      sh_printf(CLR_GREEN "AI" CLR_RESET ": Goodbye! Take care.\n\n");
      return;
    }
    if (pos == 0) {
      sh_printf(CLR_GREEN "AI" CLR_RESET ": Please tell me something.\n");
      continue;
    }

    const char *resp = NULL;

    if (resp == NULL) {
      const char *p = strstr(input, "i am ");
      if (p == input) { static char buf[256]; snprintf(buf, sizeof(buf), "Why are you %s?", p + 5); resp = buf; }
      p = strstr(input, "im ");
      if (p == input) { static char buf2[256]; snprintf(buf2, sizeof(buf2), "Why are you %s?", p + 3); resp = buf2; }
      p = strstr(input, "i m ");
      if (p == input) { static char buf3[256]; snprintf(buf3, sizeof(buf3), "Why are you %s?", p + 4); resp = buf3; }
      p = strstr(input, "i feel ");
      if (p == input) { static char buf4[256]; snprintf(buf4, sizeof(buf4), "Why do you feel %s?", p + 7); resp = buf4; }
    }

    if (resp == NULL) {
      const char *p = strstr(input, "i ");
      if (p == input) { static char buf[256]; snprintf(buf, sizeof(buf), "Why do you %s?", p + 2); resp = buf; }
    }

    if (resp == NULL && strstr(input, "sorry")) resp = "No need to apologize. What's bothering you?";
    if (resp == NULL && strstr(input, "love"))  resp = "Tell me more about your feelings.";
    if (resp == NULL && strstr(input, "hate"))  resp = "That's a strong emotion. Why do you feel that way?";
    if (resp == NULL && (strcmp(input, "yes") == 0 || strcmp(input, "yeah") == 0)) resp = "I see. Please go on.";
    if (resp == NULL && strcmp(input, "no") == 0) resp = "Why not?";
    if (resp == NULL && strstr(input, "because")) { static char buf[256]; snprintf(buf, sizeof(buf), "Is that the real reason?"); resp = buf; }
    if (resp == NULL && strstr(input, "always")) resp = "Can you think of a specific example?";
    if (resp == NULL && strstr(input, "never"))  resp = "Never? That's a strong word.";
    if (resp == NULL && strstr(input, "mother")) resp = "Tell me more about your family.";
    if (resp == NULL && strstr(input, "father")) resp = "How is your relationship with your father?";
    if (resp == NULL && strstr(input, "family")) resp = "How do you feel about your family?";
    if (resp == NULL && strstr(input, "friend")) resp = "Tell me about your friends.";
    if (resp == NULL && strstr(input, "angry"))  resp = "What made you angry?";
    if (resp == NULL && strstr(input, "sad"))    resp = "I'm sorry you're feeling sad. Can you tell me why?";
    if (resp == NULL && strstr(input, "happy"))  resp = "It's good to feel happy. What caused it?";
    if (resp == NULL && strstr(input, "dream"))  resp = "What do you think that dream means?";
    if (resp == NULL && (strstr(input, "computer") || strstr(input, "ai"))) resp = "Do computers worry you?";
    if (resp == NULL && strstr(input, "you "))   resp = "Let's talk about you, not me.";
    if (resp == NULL && strstr(input, " you"))   resp = "Let's talk about you, not me.";

    if (resp == NULL) {
      static const char *fallback[] = {
        "Tell me more about that.",
        "How does that make you feel?",
        "I understand. Please continue.",
        "Can you elaborate on that?",
        "That's interesting. Go on.",
        "What do you think about that?",
        "How long have you been feeling this way?",
      };
      resp = fallback[turns % 7];
    }

    sh_printf(CLR_GREEN "AI" CLR_RESET ": %s\n", resp);
  }
}

/*
 * cmd_cc — C 编译器前端
 */
static void cmd_cc(int argc, char *argv[]) {
  if (argc < 2) {
    sh_error("cc", "missing source file\n");
    sh_printf("usage: cc <file.c> [-o <output>]\n");
    return;
  }

  const char *src = argv[1];
  size_t len = strlen(src);
  if (len < 2 || strcmp(src + len - 2, ".c") != 0) {
    sh_error("cc", "%s: not a C source file (.c expected)\n", src);
    return;
  }

  char path[128];
  make_path(src, path, sizeof(path));
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    sh_error("cc", "%s: no such file\n", path);
    return;
  }
  close(fd);

  char *exec_argv[8];
  int ea = 0;
  exec_argv[ea++] = "/bin/cc";
  exec_argv[ea++] = path;
  for (int i = 2; i < argc && ea < 7; i++) {
    exec_argv[ea++] = argv[i];
  }
  exec_argv[ea] = NULL;
  char *exec_envp[] = { NULL };

  sh_printf("cc: compiling %s...\n", src);
  refresh_terminal();
  execve("/bin/cc", exec_argv, exec_envp);

  sh_error("cc", "compiler /bin/cc not found.\n");
  sh_printf("    Build it: cd navy-apps/apps/cc && make ISA=riscv32 install\n");
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

  const char *cmd_name = argv[0];
  if (cmd_name[0] == '!' && cmd_name[1] != '\0') {
    int n = atoi(cmd_name + 1);
    const char *replay = hist_get(n);
    if (replay) {
      sh_printf("%s\n", replay);
      sh_handle_cmd(replay);
    } else {
      sh_printf("history: no such command: %s\n", cmd_name);
    }
    return;
  }

  bool record = true;
  if (strcmp(argv[0], "history") == 0 || cmd_name[0] == '!') record = false;
  if (record) hist_add(cmd);

  if (strcmp(argv[0], "help") == 0) {
    cmd_help();
  } else if (strcmp(argv[0], "clear") == 0) {
    sh_printf("\033[2J\033[H");
  } else if (strcmp(argv[0], "pwd") == 0) {
    sh_printf("%s\n", cwd);
  } else if (strcmp(argv[0], "cd") == 0) {
    cmd_cd(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "ls") == 0) {
    cmd_ls(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "cat") == 0) {
    if (argc < 2) sh_error("cat", "missing file operand\n");
    else cmd_cat_path(argv[1]);
  } else if (strcmp(argv[0], "touch") == 0) {
    cmd_touch(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "write") == 0) {
    cmd_write_text(rest, false);
  } else if (strcmp(argv[0], "append") == 0) {
    cmd_write_text(rest, true);
  } else if (strcmp(argv[0], "rm") == 0) {
    cmd_rm(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "mkdir") == 0) {
    cmd_mkdir(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "rmdir") == 0) {
    cmd_rmdir(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "meminfo") == 0) {
    cmd_cat_path("/proc/meminfo");
  } else if (strcmp(argv[0], "free") == 0) {
    cmd_free();
  } else if (strcmp(argv[0], "ps") == 0) {
    cmd_ps();
  } else if (strcmp(argv[0], "uptime") == 0) {
    cmd_uptime();
  } else if (strcmp(argv[0], "date") == 0) {
    cmd_date(argc, argv);
  } else if (strcmp(argv[0], "history") == 0) {
    cmd_history();
  } else if (strcmp(argv[0], "redo") == 0) {
    cmd_redo(argc, argv);
  } else if (strcmp(argv[0], "hexdump") == 0) {
    cmd_hexdump(argc, argv);
  } else if (strcmp(argv[0], "tree") == 0) {
    cmd_tree(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "wc") == 0) {
    cmd_wc(argc, argv);
  } else if (strcmp(argv[0], "env") == 0) {
    cmd_env();
  } else if (strcmp(argv[0], "yes") == 0) {
    cmd_yes(argc >= 2 ? argv[1] : NULL);
  } else if (strcmp(argv[0], "shutdown") == 0 || strcmp(argv[0], "poweroff") == 0) {
    sh_printf(CLR_RED "Powering off NEMU..." CLR_RESET "\n");
    refresh_terminal();
    _syscall_(SYS_shutdown, 0, 0, 0);
  } else if (strcmp(argv[0], "run") == 0 || strcmp(argv[0], "exec") == 0) {
    cmd_run(argc - 1, argv + 1);
  } else if (strcmp(argv[0], "login") == 0) {
    if (argc < 2) { sh_error("login", "Usage: login User1 | User2 | User3\n"); }
    else {
      int uid = -1;
      for (int i = 0; i < 3; i++) if (strcmp(argv[1], user_names[i]) == 0) uid = i;
      if (uid < 0) { sh_error("login", "Unknown user. Try: User1, User2, User3\n"); }
      else if (users[uid].logged_in) { sh_printf(CLR_YELLOW "%s is already logged in elsewhere.\n" CLR_RESET, user_names[uid]); }
      else {
        if (current_user >= 0) { save_term_state(current_user); users[current_user].logged_in = false; }
        current_user = uid;
        users[uid].logged_in = true;
        if (users[uid].work_dir[0] == '\0') strcpy(users[uid].work_dir, "/");
        save_current_user();
        if (users[uid].cursor_y == 0) { users[uid].cursor_x = 0; users[uid].cursor_y = 3; }
        sh_printf("\033[2J\033[H");
        sh_banner();
        restore_term_state(uid);
        sh_printf(CLR_GREEN "Logged in as %s" CLR_RESET "\n", user_names[uid]);
        sh_prompt();
      }
    }
  } else if (strcmp(argv[0], "logout") == 0) {
    if (current_user < 0) { sh_printf(CLR_YELLOW "Not logged in.\n" CLR_RESET); }
    else {
      save_term_state(current_user);
      users[current_user].logged_in = false;
      sh_printf(CLR_YELLOW "Logged out %s" CLR_RESET "\n", user_names[current_user]);
      unlink("/tmp/.current_user");
      current_user = -1;
      sh_printf("\033[2J\033[H");
      sh_banner();
    }
  } else if (current_user < 0) {
    sh_error("shell", "Please login first: login User1 | User2 | User3\n");
  } else if (strcmp(argv[0], "demo") == 0) {
    _syscall_(SYS_demo, 0, 0, 0);
    /* 从内核固定地址读取调度日志并显示在 shell 中 */
    const char *log = (const char *)0x8FFE0000;
    if (log[0] != '\0') {
      sh_printf("%s", log);
    } else {
      sh_printf(CLR_GREEN "Demo threads finished." CLR_RESET "\n");
    }
  } else if (strcmp(argv[0], "chat") == 0 || strcmp(argv[0], "ai") == 0) {
    cmd_chat();
    sh_prompt();
  } else if (strcmp(argv[0], "cc") == 0) {
    cmd_cc(argc, argv);
  } else if (strcmp(argv[0], "echo") == 0) {
    rest = skip_space(rest);
    sh_printf("%s\n", rest);
  } else if (strcmp(argv[0], "uname") == 0) {
    sh_printf(CLR_CYAN "Nanos-lite" CLR_RESET " riscv32-nemu single-process server demo\n");
  } else {
    sh_error(argv[0], "command not found\n");
  }
}

/*
 * draw_status_bar — 在终端顶部 3 行绘制系统资源状态栏
 * 分区着色：CYAN 用户名 | GREEN 内存 | YELLOW 进程 | CYAN 运行时间
 */
static void draw_status_bar(void) {
  int tw = term->w;
  struct timeval tv; gettimeofday(&tv, NULL);
  unsigned up_s = (unsigned)tv.tv_sec;

  /* 读取内存信息 (values in kB from kernel) */
  unsigned mt_kb = 0, mu_kb = 0;
  int fd = open("/proc/meminfo", O_RDONLY);
  if (fd >= 0) {
    char buf[256]; int n = read(fd, buf, sizeof(buf)-1); close(fd);
    if (n > 0) { buf[n] = '\0';
      char *l = strtok(buf, "\n");
      while (l) {
        unsigned v;
        if (sscanf(l, "MemTotal: %u kB", &v) == 1) mt_kb = v;
        if (sscanf(l, "MemUsed:  %u kB", &v) == 1) mu_kb = v;
        l = strtok(NULL, "\n");
      }
    }
  }

  /* 进程计数 */
  int pcount = 0;
  fd = open("/proc/processes", O_RDONLY);
  if (fd >= 0) {
    char buf[512]; int n = read(fd, buf, sizeof(buf)-1); close(fd);
    if (n > 0) { buf[n] = '\0';
      char *l = strtok(buf, "\n");
      while (l) { pcount++; l = strtok(NULL, "\n"); }
      if (pcount > 0) pcount--;
    }
  }

  /* HH:MM:SS format */
  unsigned hh = up_s / 3600;
  unsigned mm = (up_s % 3600) / 60;
  unsigned ss_val = up_s % 60;

  /* 保存光标和颜色 */
  int cx = term->cursor.x, cy = term->cursor.y;
  uint8_t cf = term->col_f, cb = term->col_b;

  /* Blue background for entire status bar */
  term->col_b = 4;  /* BLUE */

  /* Row 0: spacer for clean background */
  term->col_f = 4;
  for (int x = 0; x < tw; x++) term->putch(x, 0, ' ');

  /* Row 1: section-coloured data with white separators */
  char section[64];
  int written = 0;

  /* Section 1: user name in CYAN */
  term->col_f = 6;  /* CYAN */
  snprintf(section, sizeof(section), " %s ", current_user >= 0 ? user_names[current_user] : "Login");
  for (int i = 0; section[i] && written < tw; i++) term->putch(written++, 1, section[i]);

  /* Separator */
  term->col_f = 7;  /* WHITE */
  if (written < tw) term->putch(written++, 1, '|');

  /* Section 2: memory in GREEN */
  term->col_f = 2;  /* GREEN */
  if (mt_kb >= 1024) {
    snprintf(section, sizeof(section), " Mem %u/%uM ", mu_kb / 1024, mt_kb / 1024);
  } else {
    snprintf(section, sizeof(section), " Mem %u/%uK ", mu_kb, mt_kb);
  }
  for (int i = 0; section[i] && written < tw; i++) term->putch(written++, 1, section[i]);

  /* Separator */
  term->col_f = 7;
  if (written < tw) term->putch(written++, 1, '|');

  /* Section 3: process count in YELLOW */
  term->col_f = 3;  /* YELLOW */
  snprintf(section, sizeof(section), " Procs %d ", pcount);
  for (int i = 0; section[i] && written < tw; i++) term->putch(written++, 1, section[i]);

  /* Separator */
  term->col_f = 7;
  if (written < tw) term->putch(written++, 1, '|');

  /* Section 4: uptime in CYAN */
  term->col_f = 6;
  snprintf(section, sizeof(section), " Up %02u:%02u:%02u ", hh, mm, ss_val);
  for (int i = 0; section[i] && written < tw; i++) term->putch(written++, 1, section[i]);

  /* Right side hint */
  term->col_f = 7;
  const char *hint = current_user >= 0 ? "| login/out " : "| login ";
  for (int i = 0; hint[i] && written < tw; i++) term->putch(written++, 1, hint[i]);

  /* Fill the rest with background */
  term->col_f = 4;
  for (; written < tw; written++) term->putch(written, 1, ' ');

  /* Row 2: separator line with `=` */
  term->col_f = 7;
  for (int x = 0; x < tw; x++) term->putch(x, 2, '=');

  /* 恢复光标和颜色 */
  term->col_f = cf; term->col_b = cb;
  if (cy < 3) { term->cursor.x = cx; term->cursor.y = 3; }
  else { term->cursor.x = cx; term->cursor.y = cy; }
}

void builtin_sh_run() {
  sh_banner();
  sh_prompt();

  uint32_t last_monitor_refresh = 0;

  while (1) {
    SDL_Event ev;
    if (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_KEYUP || ev.type == SDL_KEYDOWN) {
        int key = ev.key.keysym.sym;
        const uint8_t *ks = SDL_GetKeyState(NULL);

        const char *res = term->keypress(handle_key(&ev));
        if (res) {
          sh_handle_cmd(res);
          if (current_user >= 0) sh_prompt();
        }
      }
    }

    /* draw status bar every 500ms */
    {
      uint32_t now = SDL_GetTicks();
      if (now - last_monitor_refresh > 500) {
        last_monitor_refresh = now;
        draw_status_bar();
      }
    }

    refresh_terminal();
  }
}
