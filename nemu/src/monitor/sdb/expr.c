/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <memory/vaddr.h>

enum {
  TK_NOTYPE = 256, 
  TK_EQ,
  /* TODO: Add more token types */
  TK_NUM,      // decimal number
  TK_HEX,      // hexadecimal number
  TK_REG,      // register, e.g. $pc
  TK_NEQ,      // !=
  TK_AND,      // &&
  TK_NEG,      // unary minus
  TK_DEREF,    // unary dereference
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  // +   表示前面的内容出现一次或多次
  // *   表示前面的内容出现零次或多次
  // ( ) 表示分组在 C 字符串里，反斜杠本身也要转义，所以写成：
  {" +",        TK_NOTYPE}, // spaces
  {"0[xX][0-9a-fA-F]+", TK_HEX}, // hex number
  {"\\$[a-zA-Z0-9]+", TK_REG},   // register
  {"==",        TK_EQ},     // equal
  {"!=",        TK_NEQ},    // not equal
  {"&&",        TK_AND},    // logical and
  {"\\+",       '+'},       // plus
  {"-",         '-'},       // minus
  {"\\*",       '*'},       // multiply or dereference
  {"/",         '/'},       // divide
  {"\\(",       '('},       // left parenthesis
  {"\\)",       ')'},       // right parenthesis
  {"[0-9]+",    TK_NUM},    // decimal number
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[64];
} Token;

static Token tokens[256] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool check_parentheses(int p, int q);
static int find_main_operator(int p, int q);
static word_t eval(int p, int q, bool *success);

static bool is_binary_context(int type) {
  return (type == TK_NUM || type == TK_HEX || type == TK_REG || type == ')');
}

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        if (nr_token >= (int)ARRLEN(tokens)) {
          printf("too many tokens\n");
          return false;
        }

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;

          //数字、十六进制、寄存器：需要保存字符串
          case TK_NUM:
          case TK_HEX:
          case TK_REG:
            tokens[nr_token].type = rules[i].token_type;
            if (substr_len >= (int)sizeof(tokens[nr_token].str)) {
              printf("token too long: %.*s\n", substr_len, substr_start);
              return false;
            }
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token++;
            break;
          
          // 其他符号：只记录类型，不保存字符串
          case TK_EQ:
          case TK_NEQ:
          case TK_AND:
          case '+':
          case '-':
          case '*':
          case '/':
          case '(':
          case ')':
            tokens[nr_token].type = rules[i].token_type;
            tokens[nr_token].str[0] = '\0';
            nr_token++;
            break;

          default:
            TODO();
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  /* 我补充: 在 token 化之后, 识别单目运算符 TK_NEG / TK_DEREF */
  for (i = 0; i < nr_token; i++) {
    if (tokens[i].type == '-') {
      if (i == 0 || !is_binary_context(tokens[i - 1].type)) {
        tokens[i].type = TK_NEG;
      }
    }
    else if (tokens[i].type == '*') {
      if (i == 0 || !is_binary_context(tokens[i - 1].type)) {
        tokens[i].type = TK_DEREF;
      }
    }
  }

  return true;
}

static bool check_parentheses(int p, int q) {
  if (p > q) return false;
  if (tokens[p].type != '(' || tokens[q].type != ')') return false;

  int level = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') level++;
    else if (tokens[i].type == ')') level--;

    if (level < 0) return false;

    if (level == 0 && i < q) {
      return false;
    }
  }

  return level == 0;
}

static int precedence(int type) {
  switch (type) {
    case TK_AND:   return 1;
    case TK_EQ:
    case TK_NEQ:   return 2;
    case '+':
    case '-':      return 3;
    case '*':
    case '/':      return 4;
    case TK_NEG:
    case TK_DEREF: return 5;
    default:       return 100;
  }
}

static bool is_unary_operator(int type) {
  return type == TK_NEG || type == TK_DEREF;
}

static int find_main_operator(int p, int q) {
  int main_op = -1;
  int min_prec = 100;
  int level = 0;

  for (int i = p; i <= q; i++) {
    int type = tokens[i].type;

    if (type == '(') {
      level++;
      continue;
    }
    if (type == ')') {
      level--;
      continue;
    }

    if (level > 0) continue;

    switch (type) {
      case TK_AND:
      case TK_EQ:
      case TK_NEQ:
      case '+':
      case '-':
      case '*':
      case '/':
      case TK_NEG:
      case TK_DEREF: {
        int prec = precedence(type);

        if (prec < min_prec) {
          min_prec = prec;
          main_op = i;
        }
        else if (prec == min_prec) {
          /*
           * 二元左结合运算符：同优先级取右边
           * 单目前缀运算符：同优先级保留左边
           */
          if (!is_unary_operator(type)) {
            main_op = i;
          }
        }

        break;
      }

      default:
        break;
    }
  }

  return main_op;
}

static word_t eval_single_token(int idx, bool *success) {
  if (tokens[idx].type == TK_NUM) {
    return (word_t)strtoul(tokens[idx].str, NULL, 10);
  }
  else if (tokens[idx].type == TK_HEX) {
    return (word_t)strtoul(tokens[idx].str, NULL, 16);
  }
  else if (tokens[idx].type == TK_REG) {
    /* tokens[idx].str 形如 "$pc" */
    return isa_reg_str2val(tokens[idx].str + 1, success);
  }

  *success = false;
  return 0;
}

static word_t eval(int p, int q, bool *success) {
  if (!*success) return 0;

  if (p > q) {
    *success = false;
    return 0;
  }

  if (p == q) {
    return eval_single_token(p, success);
  }

  if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }

  int op = find_main_operator(p, q);
  if (op < 0) {
    *success = false;
    return 0;
  }

  if (tokens[op].type == TK_NEG) {
    word_t val = eval(op + 1, q, success);
    return -val;
  }

  if (tokens[op].type == TK_DEREF) {
    word_t addr = eval(op + 1, q, success);
    if (!*success) return 0;
    return vaddr_read(addr, 4);
  }

  word_t val1 = eval(p, op - 1, success);
  word_t val2 = eval(op + 1, q, success);

  if (!*success) return 0;

  switch (tokens[op].type) {
    case '+':   return val1 + val2;
    case '-':   return val1 - val2;
    case '*':   return val1 * val2;
    case '/':
      if (val2 == 0) {
        *success = false;
        printf("division by zero\n");
        return 0;
      }
      return val1 / val2;
    case TK_EQ:  return val1 == val2;
    case TK_NEQ: return val1 != val2;
    case TK_AND: return val1 && val2;
    default:
      *success = false;
      return 0;
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  if (nr_token == 0) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  /* 我补充: 递归求值入口 */
  return eval(0, nr_token - 1, success);
}