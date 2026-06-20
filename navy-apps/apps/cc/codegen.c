/* codegen.c — single-pass compiler + stack VM for C subset */
#include "cc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void cc_init(CC *cc, const char *s) {
  size_t len = strlen(s);
  memset(cc, 0, sizeof(*cc));
  if (len >= SRC_MAX) len = SRC_MAX - 1;
  memcpy(cc->src, s, len);
  cc->src[len] = '\0';
  cc->src_len = len;
}

/* ===== bytecode buffer ===== */
static uint8_t *bc; static int bclen, bccap;
static void bce(uint8_t b)  { if (bclen < bccap) bc[bclen++] = b; }
static void bci(int32_t v)  { uint8_t *p = (uint8_t *)&v; bce(p[0]); bce(p[1]); bce(p[2]); bce(p[3]); }
static int  bcp(void)       { return bclen; }
static void bcpatch(int pos, int32_t v) {
  uint8_t *p = (uint8_t *)&v;
  bc[pos]=p[0]; bc[pos+1]=p[1]; bc[pos+2]=p[2]; bc[pos+3]=p[3];
}

/* ===== VM output buffer (read by main.c after vm_execute) ===== */
char vm_out_buf[2048];
int  vm_out_pos;
void vm_out_reset(void) { vm_out_pos = 0; vm_out_buf[0] = '\0'; }
static void vm_putc(char c) {
  if (vm_out_pos < (int)sizeof(vm_out_buf) - 1) vm_out_buf[vm_out_pos++] = c;
}
static void vm_puts(const char *s) {
  while (*s) vm_putc(*s++);
}

/* ===== string table ===== */
static char st[STR_COUNT][128]; static int nst;
static int st_add(const char *s) {
  for (int i=0; i<nst; i++) if (strcmp(st[i],s)==0) return i;
  if (nst<STR_COUNT) { strncpy(st[nst],s,127); return nst++; }
  return 0;
}

/* ===== function table ===== */
static Func ft[FUNC_MAX];
static int nft, curf;
static char vn[VARS_MAX][32]; static int nv; /* local var names for current function */

static int var_slot(const char *name) {
  for (int i=0; i<curf; i++) if (nv>0) for (int j=0; j<nv; j++) /* search current func */
    ;
  /* search current function's locals */
  for (int i=0; i<nv; i++) if (strcmp(vn[i],name)==0) return i;
  strncpy(vn[nv], name, 31);
  return nv++;
}

static int func_idx(const char *name) {
  for (int i=0; i<nft; i++) if (strcmp(ft[i].name,name)==0) return i;
  return -1;
}

/* ===== lexer state ===== */
static const char *src; static int spos;
static int    tok_i;  static char tok_s[64]; static int tok_k;
#define TK_EOF 0
#define TK_INT 1
#define TK_IF 2
#define TK_ELSE 3
#define TK_WHILE 4
#define TK_RETURN 5
#define TK_ID  6
#define TK_NUM 7
#define TK_STR 8
#define TK_LP  9  /* ( */
#define TK_RP  10 /* ) */
#define TK_LB  11 /* { */
#define TK_RB  12 /* } */
#define TK_SEMI 13
#define TK_COMMA 14
#define TK_ASSIGN 15 /* = */
#define TK_PLUS 16
#define TK_MINUS 17
#define TK_STAR 18
#define TK_SLASH 19
#define TK_PERCENT 20
#define TK_EQ   21
#define TK_NE   22
#define TK_LT   23
#define TK_GT   24
#define TK_LE   25
#define TK_GE   26
#define TK_AND  27
#define TK_OR   28
#define TK_NOT  29
#define TK_VOID 30

static void skip_ws(void) {
  while (src[spos]) {
    char c=src[spos];
    if (c==' '||c=='\t'||c=='\n'||c=='\r'){spos++;continue;}
    if (c=='/'&&src[spos+1]=='/'){spos+=2;while(src[spos]&&src[spos]!='\n')spos++;continue;}
    if (c=='/'&&src[spos+1]=='*'){spos+=2;while(src[spos]&&!(src[spos]=='*'&&src[spos+1]=='/'))spos++;if(src[spos])spos+=2;continue;}
    /* skip #include, #define etc. — treat as line comment */
    if (c=='#'){spos++;while(src[spos]&&src[spos]!='\n')spos++;continue;}
    break;
  }
}
static void next_tok(void) {
  skip_ws();
  if (!src[spos]) { tok_k=TK_EOF; return; }
  char c=src[spos];
  if (c=='"') {
    spos++; int i=0;
    while (src[spos]&&src[spos]!='"') {
      if (src[spos]=='\\'){spos++; char e=src[spos]; if(e=='n')tok_s[i++]='\n'; else if(e=='t')tok_s[i++]='\t'; else tok_s[i++]=e;}
      else tok_s[i++]=src[spos];
      spos++; if(i>=63)break;
    }
    if(src[spos]=='"')spos++;
    tok_s[i]=0; tok_k=TK_STR; return;
  }
  if (c>='0'&&c<='9'){ int v=0; while(src[spos]>='0'&&src[spos]<='9'){v=v*10+(src[spos]-'0');spos++;} tok_i=v; tok_k=TK_NUM; return; }
  if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'){
    int i=0;
    while(((src[spos]>='a'&&src[spos]<='z')||(src[spos]>='A'&&src[spos]<='Z')||(src[spos]>='0'&&src[spos]<='9')||src[spos]=='_')&&i<63) tok_s[i++]=src[spos++];
    tok_s[i]=0;
    if (strcmp(tok_s,"int")==0) tok_k=TK_INT; else if(strcmp(tok_s,"if")==0) tok_k=TK_IF;
    else if(strcmp(tok_s,"else")==0) tok_k=TK_ELSE; else if(strcmp(tok_s,"while")==0) tok_k=TK_WHILE;
    else if(strcmp(tok_s,"return")==0) tok_k=TK_RETURN; else if(strcmp(tok_s,"void")==0) tok_k=TK_VOID;
    else tok_k=TK_ID;
    return;
  }
  spos++;
  if (c=='(') tok_k=TK_LP; else if(c==')') tok_k=TK_RP; else if(c=='{') tok_k=TK_LB;
  else if(c=='}') tok_k=TK_RB; else if(c==';') tok_k=TK_SEMI; else if(c==',') tok_k=TK_COMMA;
  else if(c=='+') tok_k=TK_PLUS; else if(c=='-') tok_k=TK_MINUS; else if(c=='*') tok_k=TK_STAR;
  else if(c=='/') tok_k=TK_SLASH; else if(c=='%') tok_k=TK_PERCENT;
  else if(c=='='){ if(src[spos]=='='){spos++;tok_k=TK_EQ;}else tok_k=TK_ASSIGN; }
  else if(c=='!'){ if(src[spos]=='='){spos++;tok_k=TK_NE;}else tok_k=TK_NOT; }
  else if(c=='<'){ if(src[spos]=='='){spos++;tok_k=TK_LE;}else tok_k=TK_LT; }
  else if(c=='>'){ if(src[spos]=='='){spos++;tok_k=TK_GE;}else tok_k=TK_GT; }
  else if(c=='&'){ if(src[spos]=='&'){spos++;tok_k=TK_AND;} }
  else if(c=='|'){ if(src[spos]=='|'){spos++;tok_k=TK_OR;} }
  else next_tok(); /* skip unknown */
}

static int peek(void) { return tok_k; }
static void expect(int k) { if (tok_k==k) next_tok(); }

/* ===== forward decls ===== */
static void parse_expr(void);

/* ===== expression parser (emits bytecode directly) ===== */
static void parse_primary(void) {
  if (tok_k==TK_NUM){ bce(OP_PUSH_IMM); bci(tok_i); next_tok(); }
  else if (tok_k==TK_STR){ int si=st_add(tok_s); bce(OP_PUSH_IMM); bci(si); next_tok(); }
  else if (tok_k==TK_ID){
    char name[64]; strncpy(name,tok_s,63); next_tok();
    if (tok_k==TK_LP){ /* function call */
      next_tok();
      int nargs=0, argpos[16];
      if (tok_k!=TK_RP){ do { argpos[nargs++]=bcp(); parse_expr(); } while(tok_k==TK_COMMA&&(next_tok(),1)); }
      expect(TK_RP);
      /* push args in reverse (they're on stack in order, need rev for VM) */
      /* actually VM expects args in order: fmt, a1, a2... so we push them in order */
      int fidx=func_idx(name);
      /* emit args in forward order (they'll be popped in forward order by VM) */
      /* wait — we already pushed args while parsing. They're on stack in arg order.
         CALL expects: stack has [arg0 arg1 ... argN-1], then CALL pops nargs and restores.
         We need to make sure args are in correct order after all the parsing. */
      bce(OP_CALL);
      if (strcmp(name,"printf")==0) bci(-1); else bci(fidx);
      bci(nargs);
    } else {
      /* variable */
      int slot=var_slot(name);
      bce(OP_PUSH_VAR); bci(slot);
    }
  }
  else if (tok_k==TK_LP){ next_tok(); parse_expr(); expect(TK_RP); }
}

static void parse_unary(void) {
  if (tok_k==TK_MINUS){ next_tok(); parse_unary(); bce(OP_NEG); }
  else if (tok_k==TK_NOT){ next_tok(); parse_unary(); bce(OP_PUSH_IMM); bci(0); bce(OP_CMP_EQ); }
  else parse_primary();
}

static void parse_mul(void) {
  parse_unary();
  while (tok_k==TK_STAR||tok_k==TK_SLASH||tok_k==TK_PERCENT){
    int op=tok_k; next_tok(); parse_unary();
    if (op==TK_STAR) bce(OP_MUL); else if(op==TK_SLASH) bce(OP_DIV); else bce(OP_MOD);
  }
}

static void parse_add(void) {
  parse_mul();
  while (tok_k==TK_PLUS||tok_k==TK_MINUS){
    int op=tok_k; next_tok(); parse_mul();
    if (op==TK_PLUS) bce(OP_ADD); else bce(OP_SUB);
  }
}

static void parse_cmp(void) {
  parse_add();
  while (tok_k==TK_LT||tok_k==TK_GT||tok_k==TK_LE||tok_k==TK_GE||tok_k==TK_EQ||tok_k==TK_NE){
    int op=tok_k; next_tok(); parse_add();
    if(op==TK_LT)bce(OP_CMP_LT); else if(op==TK_GT)bce(OP_CMP_GT);
    else if(op==TK_LE)bce(OP_CMP_LE); else if(op==TK_GE)bce(OP_CMP_GE);
    else if(op==TK_EQ)bce(OP_CMP_EQ); else bce(OP_CMP_NE);
  }
}

static void parse_and(void) {
  parse_cmp();
  while (tok_k==TK_AND){ next_tok(); parse_cmp(); bce(OP_MUL); /* && = logical AND, use MUL as workaround: both non-zero => non-zero */ }
}

static void parse_or(void) {
  parse_and();
  while (tok_k==TK_OR){ next_tok(); parse_and();
    /* a || b => (a + b) != 0 approximately */
    bce(OP_ADD);
  }
}

static void parse_expr(void) { parse_or(); }

/* ===== statement parser ===== */
static void parse_stmt(void);

static void parse_block(void) {
  expect(TK_LB);
  while (tok_k!=TK_RB && tok_k!=TK_EOF) parse_stmt();
  expect(TK_RB);
}

static void parse_stmt(void) {
  if (tok_k==TK_INT){ /* int x; or int x = expr; */
    next_tok();
    char vname[64]; strncpy(vname,tok_s,63); next_tok();
    int slot=var_slot(vname);
    if (tok_k==TK_ASSIGN){ next_tok(); parse_expr(); bce(OP_POP_VAR); bci(slot); }
    expect(TK_SEMI);
  }
  else if (tok_k==TK_IF){
    next_tok(); expect(TK_LP); parse_expr(); expect(TK_RP);
    int jf=bcp(); bci(0); /* placeholder JMP_FALSE */
    parse_stmt();
    if (tok_k==TK_ELSE){
      int je=bcp(); bci(0); /* placeholder JMP */
      bcpatch(jf, bclen - jf - 4);
      next_tok(); parse_stmt();
      bcpatch(je, bclen - je - 4);
    } else {
      bcpatch(jf, bclen - jf - 4);
    }
  }
  else if (tok_k==TK_WHILE){
    int loop_start=bcp();
    next_tok(); expect(TK_LP); parse_expr(); expect(TK_RP);
    int jf=bcp(); bci(0);
    parse_stmt();
    bce(OP_JMP); bci(loop_start - bclen - 4);
    bcpatch(jf, bclen - jf - 4);
  }
  else if (tok_k==TK_RETURN){
    next_tok();
    if (tok_k!=TK_SEMI) parse_expr(); else { bce(OP_PUSH_IMM); bci(0); }
    bce(OP_RET);
    expect(TK_SEMI);
  }
  else if (tok_k==TK_LB) parse_block();
  else if (tok_k==TK_ID){
    char name[64]; strncpy(name,tok_s,63); next_tok();
    if (tok_k==TK_ASSIGN){ /* x = expr; */
      next_tok(); parse_expr();
      int slot=var_slot(name);
      bce(OP_POP_VAR); bci(slot);
      expect(TK_SEMI);
    } else if (tok_k==TK_LP){ /* func(); */
      next_tok();
      int nargs=0;
      if (tok_k!=TK_RP){ do { parse_expr(); nargs++; } while(tok_k==TK_COMMA&&(next_tok(),1)); }
      expect(TK_RP);
      int fidx=func_idx(name);
      bce(OP_CALL);
      if (strcmp(name,"printf")==0) bci(-1); else bci(fidx);
      bci(nargs);
      bce(OP_POP); /* discard return value */
      expect(TK_SEMI);
    } else { /* skip unknown; */ while(tok_k!=TK_SEMI&&tok_k!=TK_EOF)next_tok(); expect(TK_SEMI); }
  }
  else { /* skip */ while(tok_k!=TK_SEMI&&tok_k!=TK_EOF&&tok_k!=TK_RB)next_tok(); expect(TK_SEMI); }
}

/* ===== compile entry ===== */
int cc_compile(CC *cc, VM *vm) {
  src = cc->src; spos = 0;
  nft = 0; nst = 0; bclen = 0; bccap = CODE_MAX;
  bc = vm->code;
  memset(ft, 0, sizeof(ft));
  memset(st, 0, sizeof(st));

  next_tok();

  /* parse functions: int name(params) { body } */
  while (tok_k != TK_EOF) {
    if (tok_k != TK_INT) { next_tok(); continue; }
    next_tok();
    if (tok_k != TK_ID) { next_tok(); continue; }

    char fname[64]; strncpy(fname, tok_s, 63); next_tok();
    expect(TK_LP);

    /* params: int a, int b, ...  OR  void  OR  empty */
    int nparams = 0;
    nv = 0; /* reset local vars */
    if (tok_k == TK_INT) {
      next_tok();
      if (tok_k == TK_ID) { var_slot(tok_s); nparams++; next_tok(); }
      while (tok_k == TK_COMMA) { next_tok(); expect(TK_INT); if (tok_k==TK_ID) { var_slot(tok_s); nparams++; next_tok(); } }
    } else if (tok_k == TK_VOID) {
      next_tok();
    }
    expect(TK_RP);

    /* record function */
    curf = nft;
    strncpy(ft[curf].name, fname, 31);
    ft[curf].nargs = nparams;
    ft[curf].code_start = bclen;

    /* body */
    parse_block();

    /* auto-return if no explicit return */
    bce(OP_PUSH_IMM); bci(0);
    bce(OP_RET);

    ft[curf].code_len = bclen - ft[curf].code_start;
    ft[curf].nlocals = nv;
    nft++;
  }

  /* Copy data to VM */
  vm->code_len = bclen;
  vm->nfuncs = nft;
  for (int i = 0; i < nft; i++) vm->funcs[i] = ft[i];
  vm->nstrs = nst;
  for (int i = 0; i < nst; i++) strncpy(vm->str_table[i], st[i], 127);
  vm->entry_func = -1;
  for (int i = 0; i < nft; i++) if (strcmp(ft[i].name, "main") == 0) vm->entry_func = i;
  return 0;
}

/* ===== VM execute ===== */
int vm_execute(VM *vm) {
  if (vm->entry_func < 0) { printf("cc: no main()\n"); return -1; }

  int *s = vm->stack;
  int sp = 0;
  uint8_t *c = vm->code;
  int pc = vm->funcs[vm->entry_func].code_start;
  int loc[VARS_MAX] = {0};

  int call_stack_pc[16], call_stack_sp[16], call_depth = 0;

  int steps = 0;
  while (steps++ < 5000000) {
    uint8_t op = c[pc++];

    switch (op) {
      case OP_PUSH_IMM: { int32_t v; memcpy(&v,c+pc,4); pc+=4; s[sp++]=v; break; }
      case OP_PUSH_VAR: { int32_t i; memcpy(&i,c+pc,4); pc+=4; s[sp++]=(i<VARS_MAX)?loc[i]:0; break; }
      case OP_POP_VAR:  { int32_t i; memcpy(&i,c+pc,4); pc+=4; if(sp>0&&i<VARS_MAX)loc[i]=s[--sp]; break; }
      case OP_ADD: if(sp>=2){int b=s[--sp];s[sp-1]+=b;}break;
      case OP_SUB: if(sp>=2){int b=s[--sp];s[sp-1]-=b;}break;
      case OP_MUL: if(sp>=2){int b=s[--sp];s[sp-1]*=b;}break;
      case OP_DIV: if(sp>=2){int b=s[--sp];if(b)s[sp-1]/=b;}break;
      case OP_MOD: if(sp>=2){int b=s[--sp];if(b)s[sp-1]%=b;}break;
      case OP_NEG: if(sp>0)s[sp-1]=-s[sp-1];break;
      case OP_CMP_EQ:if(sp>=2){int b=s[--sp];s[sp-1]=(s[sp-1]==b);}break;
      case OP_CMP_NE:if(sp>=2){int b=s[--sp];s[sp-1]=(s[sp-1]!=b);}break;
      case OP_CMP_LT:if(sp>=2){int b=s[--sp];s[sp-1]=(s[sp-1]<b);}break;
      case OP_CMP_GT:if(sp>=2){int b=s[--sp];s[sp-1]=(s[sp-1]>b);}break;
      case OP_CMP_LE:if(sp>=2){int b=s[--sp];s[sp-1]=(s[sp-1]<=b);}break;
      case OP_CMP_GE:if(sp>=2){int b=s[--sp];s[sp-1]=(s[sp-1]>=b);}break;
      case OP_JMP:    {int32_t o;memcpy(&o,c+pc,4);pc+=4;pc+=o;break;}
      case OP_JMP_FALSE:{int32_t o;memcpy(&o,c+pc,4);pc+=4;if(sp>0&&!s[--sp])pc+=o;break;}
      case OP_DUP: if(sp>0&&sp<STACK_MAX)s[sp++]=s[sp-1];break;
      case OP_POP: if(sp>0)sp--;break;
      case OP_RET: {
        int rv = sp>0?s[--sp]:0;
        if (call_depth>0) {
          call_depth--;
          sp = call_stack_sp[call_depth];
          pc = call_stack_pc[call_depth];
          s[sp++] = rv;
        } else { return rv; }
        break;
      }
      case OP_CALL: {
        int32_t callee,nargs;
        memcpy(&callee,c+pc,4);pc+=4;
        memcpy(&nargs,c+pc,4);pc+=4;

        if (callee==-1) {
          /* printf(args...) — args on stack: fmt_idx, a1, a2... */
          int fmt_idx = s[sp-nargs];
          const char *fmt = (fmt_idx>=0&&fmt_idx<vm->nstrs)?vm->str_table[fmt_idx]:"";
          int ai=0;
          for (const char *p=fmt;*p;p++) {
            if (*p=='%'&&*(p+1)=='d'){p++;
              int v=(ai+1<nargs)?s[sp-nargs+1+ai]:0;ai++;
              char buf[16];int i=15;buf[--i]=0;int neg=v<0;if(neg)v=-v;
              do{buf[--i]='0'+(v%10);v/=10;}while(v);
              if(neg)buf[--i]='-';
              vm_puts(buf+i);
            }else vm_putc(*p);
          }
          sp-=nargs;
          s[sp++]=0;
        } else if (callee>=0&&callee<vm->nfuncs) {
          /* save state */
          call_stack_pc[call_depth]=pc;
          call_stack_sp[call_depth]=sp-nargs;
          /* copy args to callee locals */
          for (int i=0;i<nargs&&i<ft[callee].nargs;i++)
            loc[i]=s[sp-nargs+i];
          call_depth++;
          pc=vm->funcs[callee].code_start;
        }
        break;
      }
      case OP_HALT: return sp>0?s[sp-1]:0;
    }
  }
  printf("cc: VM step limit\n");
  return -1;
}
