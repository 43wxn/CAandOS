/* cc.h — mini C compiler + VM definitions */
#ifndef CC_H
#define CC_H

#include <stdint.h>

/* limits */
#define CODE_MAX  4096
#define FUNC_MAX  16
#define VARS_MAX  256
#define STACK_MAX 256
#define STR_COUNT 64
#define SRC_MAX   8192

/* opcodes */
enum {
  OP_PUSH_IMM, OP_PUSH_VAR, OP_POP_VAR, OP_PUSH_ARG,
  OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG,
  OP_CMP_EQ, OP_CMP_NE, OP_CMP_LT, OP_CMP_GT, OP_CMP_LE, OP_CMP_GE,
  OP_JMP, OP_JMP_FALSE,
  OP_CALL, OP_RET,
  OP_DUP, OP_POP, OP_HALT,
};

/* function descriptor */
typedef struct {
  char name[32];
  int  code_start, code_len, nargs, nlocals;
} Func;

/* VM state (also used as bytecode file format) */
typedef struct {
  uint8_t code[CODE_MAX];
  int     code_len;
  Func    funcs[FUNC_MAX];
  int     nfuncs;
  char    str_table[STR_COUNT][128];
  int     nstrs;
  int     entry_func;
  int     stack[STACK_MAX];  /* runtime */
  int     sp, pc;
  int     locals[VARS_MAX];
} VM;

/* compiler front-end state */
typedef struct {
  char src[SRC_MAX];
  int  src_len;
} CC;

void cc_init(CC *cc, const char *s);
int  cc_compile(CC *cc, VM *vm);
int  vm_execute(VM *vm);

#endif
