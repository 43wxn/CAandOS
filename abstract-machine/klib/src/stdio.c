#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>
#include <limits.h>
#include <stdbool.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

static void emit_char(char **out, char *end, int *cnt, char ch, bool bounded) {
  if (!bounded || *out < end) {
    *(*out)++ = ch;
  }
  (*cnt)++;
}

static void emit_str(char **out, char *end, int *cnt, const char *s, bool bounded) {
  /*
   * Guard against obviously invalid pointers that are not NULL.
   * Any address below 64 KB is certainly not a valid string pointer
   * on any reasonable platform and almost always indicates a bug
   * (e.g. a small integer being passed as a %s argument).
   */
  if (s == NULL || (uintptr_t)s < 0x10000) {
    if (s != NULL) {
      /* Only print the diagnostic once; the value is useful for debugging. */
      static int warn_cnt = 0;
      if (warn_cnt < 8) {
        printf("[emit_str] WARNING: invalid string pointer %p\n", s);
        warn_cnt++;
      }
    }
    s = "(null)";
  }
  while (*s) {
    emit_char(out, end, cnt, *s++, bounded);
  }
}

static void emit_uint_base(char **out, char *end, int *cnt, unsigned int num,
                           unsigned int base, int uppercase, bool bounded) {
  char buf[32];
  int i = 0;

  if (num == 0) {
    emit_char(out, end, cnt, '0', bounded);
    return;
  }

  while (num > 0) {
    unsigned int digit = num % base;
    if (digit < 10) {
      buf[i++] = '0' + digit;
    } else {
      buf[i++] = (uppercase ? 'A' : 'a') + (digit - 10);
    }
    num /= base;
  }

  while (i > 0) {
    emit_char(out, end, cnt, buf[--i], bounded);
  }
}

static void emit_int(char **out, char *end, int *cnt, int num, bool bounded) {
  unsigned int u;
  if (num < 0) {
    emit_char(out, end, cnt, '-', bounded);
    u = (unsigned int)(-(num + 1)) + 1;  // 避免 INT_MIN 溢出
  } else {
    u = (unsigned int)num;
  }
  emit_uint_base(out, end, cnt, u, 10, 0, bounded);
}

static int format_to_buffer(char *out, size_t n, const char *fmt, va_list ap) {
  char *p = out;
  char *end = NULL;
  int cnt = 0;

  bool discard_only = (n == 0);
  bool bounded = !(n == 0 || n == (size_t)-1);

  if (discard_only) {
    p = NULL;
    end = NULL;
  } else if (bounded) {
    end = out + n - 1;  // 留一个给 '\0'
  } else {
    // 无界模式（供 vsprintf 使用）
    end = NULL;
  }

  while (*fmt) {
    if (*fmt != '%') {
      if (!discard_only) emit_char(&p, end, &cnt, *fmt, bounded);
      else cnt++;
      fmt++;
      continue;
    }

    fmt++;  // 跳过 '%'
    if (*fmt == '\0') break;

    switch (*fmt) {
      case 'd':
      case 'i': {
        int v = va_arg(ap, int);
        if (!discard_only) emit_int(&p, end, &cnt, v, bounded);
        else {
          char tmp[32];
          char *tp = tmp;
          emit_int(&tp, NULL, &cnt, v, false);
        }
        break;
      }

      case 'u': {
        unsigned int v = va_arg(ap, unsigned int);
        if (!discard_only) emit_uint_base(&p, end, &cnt, v, 10, 0, bounded);
        else {
          char tmp[32];
          char *tp = tmp;
          emit_uint_base(&tp, NULL, &cnt, v, 10, 0, false);
        }
        break;
      }

      case 'x': {
        unsigned int v = va_arg(ap, unsigned int);
        if (!discard_only) emit_uint_base(&p, end, &cnt, v, 16, 0, bounded);
        else {
          char tmp[32];
          char *tp = tmp;
          emit_uint_base(&tp, NULL, &cnt, v, 16, 0, false);
        }
        break;
      }

      case 'X': {
        unsigned int v = va_arg(ap, unsigned int);
        if (!discard_only) emit_uint_base(&p, end, &cnt, v, 16, 1, bounded);
        else {
          char tmp[32];
          char *tp = tmp;
          emit_uint_base(&tp, NULL, &cnt, v, 16, 1, false);
        }
        break;
      }

      case 'p': {
        uintptr_t v = (uintptr_t)va_arg(ap, void *);
        if (!discard_only) {
          emit_char(&p, end, &cnt, '0', bounded);
          emit_char(&p, end, &cnt, 'x', bounded);
          emit_uint_base(&p, end, &cnt, (unsigned int)v, 16, 0, bounded);
        } else {
          cnt += 2;
          char tmp[32];
          char *tp = tmp;
          emit_uint_base(&tp, NULL, &cnt, (unsigned int)v, 16, 0, false);
        }
        break;
      }

      case 's': {
        const char *s = va_arg(ap, const char *);
        /* Catch obviously bogus string pointers early for debugging. */
        if (s != NULL && (uintptr_t)s < 0x10000) {
          static int warn_s = 0;
          if (warn_s < 5) {
            printf("[format_to_buffer %%s] WARNING: string ptr %p, fmt near \"%s\"\n",
                   s, fmt > (const char *)0x80000000 ? fmt : "(unknown)");
            warn_s++;
          }
        }
        if (!discard_only) emit_str(&p, end, &cnt, s, bounded);
        else {
          if (s == NULL) s = "(null)";
          while (*s) { cnt++; s++; }
        }
        break;
      }

      case 'c': {
        char ch = (char)va_arg(ap, int);
        if (!discard_only) emit_char(&p, end, &cnt, ch, bounded);
        else cnt++;
        break;
      }

      case '%': {
        if (!discard_only) emit_char(&p, end, &cnt, '%', bounded);
        else cnt++;
        break;
      }

      default: {
        if (!discard_only) {
          emit_char(&p, end, &cnt, '%', bounded);
          emit_char(&p, end, &cnt, *fmt, bounded);
        } else {
          cnt += 2;
        }
        break;
      }
    }

    fmt++;
  }

  if (!discard_only) {
    *p = '\0';
  }

  return cnt;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  va_list aq;
  va_copy(aq, ap);
  int ret = format_to_buffer(out, n, fmt, aq);
  va_end(aq);
  return ret;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  va_list aq;
  va_copy(aq, ap);
  int ret = format_to_buffer(out, (size_t)-1, fmt, aq);
  va_end(aq);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}

int printf(const char *fmt, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  for (int i = 0; buf[i] != '\0'; i++) {
    putch(buf[i]);
  }
  return ret;
}

#endif