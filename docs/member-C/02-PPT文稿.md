# ICS2025 答辩 PPT 文稿 — 成员 C

> **共 18 页，建议 8-10 分钟**
> **核心原则：** 每页一个主题，大量留白，代码用等宽字体，深色背景 + 青色强调色

---

## 第 1 页 · 封面

**标题：** nanos-lite 用户态应用生态构建

**副标题：** ICS2025 操作系统课程设计 · 成员 C

**底部：** 指导教师：XXX | 2026 年 6 月

> 口播（10s）：各位老师好，我是成员 C。今天展示我在 nanos-lite 中构建的用户态应用和内核定制工作。

---

## 第 2 页 · 团队分工

**标题：** 三人接力 · 我在第三棒

```
成员 A              成员 B              成员 C（我）
──────             ──────             ──────
系统启动           内存管理            ELF 加载
中断 & syscall     RAMFS 文件系统      Shell 命令
主线集成           /proc 信息          用户程序
  │                  │                 演示闭环
  ▼                  ▼                    ▼
内核可启动        文件读写可用       ★ 完整可演示系统
```

> 口播（15s）：项目采用 A→B→C 接力。A 搭好内核和系统调用，B 完成内存和文件系统，我在他们的基础上构建所有用户可见的功能。我的工作直接面向答辩展示。

---

## 第 3 页 · 工作全景

**标题：** 课程要求 + 超出范围的创新

**左侧 — 课程基线（全部完成 ✅）：**

- ELF 加载（PT_LOAD + BSS + e_entry + argc/argv）
- 12 个基础命令（help/ls/cat/touch/write/append/rm/echo/meminfo/uptime/shutdown/ps）
- ≥5 个用户程序运行（hello/timer-test/file-test/pal/bird）
- 演示脚本与答辩材料

**右侧 — ⭐ 创新（超出课程要求）：**

- **Mini C 编译器**：词法分析(30 Token) + 递归下降解析 + 栈式 VM(22 指令)
- **ELIZA AI 对话**：MIT 1966 经典算法，10+ 情感模式
- **进程调度器**：fork() + 内核线程 + Round-Robin + 现场 ESC 停止演示
- **多用户系统**：3 用户状态隔离 + execve 登录持久化
- **30+ Shell 命令** + 实时状态栏 + 9 项界面美化
- **10 个 C 测试** + **12 个 Bug 修复**

> 口播（25s）：课程要求 ELF 加载、12 个 shell 命令和 5 个用户程序——全部完成。在此基础上，我额外实现了 C 编译器、AI 对话、进程调度器、多用户系统等 8 项创新。下面逐项展示。

---

## 第 4 页 · ELF 加载器

**标题：** 基线工作 — ELF 加载 + argc/argv 传递

```
execve("/bin/hello", argv, envp)
  │
  ▼
loader.c（我的实现）:
  1. 读 ELF header → 校验 magic (0x7f 'E' 'L' 'F')
  2. 遍历 PT_LOAD 段 → memcpy 到 p_vaddr
  3. memset BSS 区域 → 页对齐 brk
  4. 写 argc + argv 到 0x8FFF0000
  5. 跳转到 e_entry
  │
  ▼
crt0.c: 从 0x8FFF0000 读回 → main(argc, argv, envp)
```

**关键设计：** 内核与用户态通过固定地址 0x8FFF0000 传递参数，crt0 启动时重建 argv 数组。

> 口播（25s）：ELF 加载器解析 ELF 头、加载所有 PT_LOAD 段、清零 BSS、页对齐 brk。关键是 argc/argv 传递——内核将参数写入固定地址，C 运行时启动后读回并重建 argv。这使得 `run edit /note.txt` 这类带参数的程序执行成为可能。

---

## 第 5 页 · Shell 命令全景

**标题：** 30+ Shell 命令

| 课程基线（12 个）✅ | ⭐ 我扩展的（18 个） |
|---------------------|---------------------|
| help / clear / pwd / echo | **cd / tree / wc / hexdump / date** |
| ls / cat | **history / redo / !N / env / yes** |
| meminfo / uptime / shutdown | **free(内存条) / mkdir / rmdir** |
| touch / write / append / rm | **login / logout（多用户）** |
| run ×5 程序 | **cc（编译器）/ chat（AI）/ demo（调度）** |

**所有命令通过 `sh_handle_cmd()` 统一分发，argc/argv 参数解析。**

> 口播（20s）：课程要求 12 个，我实现了 30+。所有命令由统一的 dispatch 函数处理，支持真正的命令行参数解析——不是简单字符串匹配。

---

## 第 6 页 · ⭐ 进程调度器（现场演示）

**标题：** 创新 — Round-Robin 进程调度器

**演示：** 在 Shell 中输入 `demo`，NEMU 窗口中直接看到：

```
=== Round-Robin Scheduler Demo ===
3 threads running continuously...
Press ESC in NEMU window to stop

[logger] tick=0
[worker] tick=0
[watchdog] tick=0
[logger] tick=1        ← tick 持续递增
[worker] tick=1        ← 三个线程交替输出
[watchdog] tick=1      ← 证明 RR 轮转正确
...
← 按 ESC 停止
=== Demo finished, back to shell ===
```

**核心实现：** `proc_schedule()` 遍历 8 槽位 PCB 数组 → 跳过当前进程 → 返回第一个 RUNNABLE 进程的 Context* → trap.S 通过 `mret` 切换。

> 口播（50s）：这是最具说服力的现场演示。输入 demo，三个线程的日志交替出现——这直接证明了 Round-Robin 调度器在正确工作。按 ESC 停止后 shell 正常恢复。调度器的核心是 proc_schedule 函数——遍历 PCB 数组，跳过当前进程，找到第一个就绪进程，返回其 Context 指针，由 trap.S 的 mret 完成切换。

---

## 第 7 页 · ⭐ 上下文切换（技术深度）

**标题：** RISC-V 汇编级上下文切换

```
trap.S 核心代码（86 行汇编）：

__am_asm_trap:
  addi sp, sp, -144          ← Context = 144 字节
  sw x1,4(sp) ... sw x31,124(sp)  ← 保存 32 GPR
  csrr t0, mcause            ← 保存异常原因
  csrr t1, mstatus           ← 保存机器状态
  csrr t2, mepc              ← 保存返回地址
  mv a0, sp                  ← ★1 传给调度器
  call __am_irq_handle       ← schedule() 选进程
  mv sp, a0                  ← ★2 接收新 Context*
  csrw mepc, t2              ← 设置目标进程返回地址
  lw x1,4(sp) ... lw x31,124(sp)  ← 恢复目标进程寄存器
  mret                       ← ★3 跳转到目标进程!
```

**三步完成进程切换：** ① mv a0,sp ② mv sp,a0 ③ mret

> 口播（35s）：我逐行分析了 trap.S 的汇编代码。关键三步：mv a0,sp 把当前进程的 Context 指针传给 C 调度器；mv sp,a0 接收调度器返回的目标进程 Context 指针；mret 跳转到目标进程。如果调度器返回另一个进程的 Context，mret 就完成了进程切换——这是操作系统最底层的魔法。

---

## 第 8 页 · ⭐⭐ Mini C 编译器

**标题：** 核心创新 — 手写 C 编译器

```
C 源码 (.c)
  │
  ▼ 词法分析器 (手写, 30 Token)
Token: int → main → ( → ) → { → return → 0 → ; → }
  │
  ▼ 递归下降解析器 (优先级爬升)
primary → unary → mul → add → cmp → and → or
支持: 变量/赋值/if/else/while/函数/递归/return
  │
  ▼ 代码生成器 (22 条自定义 Opcode)
PUSH/POP | ADD/SUB/MUL/DIV/MOD | CMP_EQ/NE/LT/GT/LE/GE
JMP/JMP_FALSE | CALL/RET | HALT
可修补跳转 → 实现 if/while 控制流
  │
  ▼ 栈式 VM
256 深度栈 | 500 万步保护 | printf() 内建
```

> 口播（55s）：这是我的核心创新。手写词法分析器识别 30 种 Token，递归下降解析器通过优先级爬升处理运算符优先级，代码生成器输出 22 条自定义字节码指令，栈式 VM 解释执行。整个编译器在操作系统用户态运行。if/else 和 while 通过可修补跳转实现——先生成占位指令，解析完条件块后回填真实跳转地址。

---

## 第 9 页 · ⭐⭐ 编译器能力证明

**标题：** 现场编译执行 C 程序

```c
int add(int a, int b) { return a + b; }

int fib(int n) {              // 递归
    if (n < 2) return n;
    return fib(n-1) + fib(n-2);
}

int is_prime(int n) {         // 循环+条件
    int d = 2;
    while (d * d < n + 1) {
        if (n % d == 0) return 0;
        d = d + 1;
    }
    return 1;
}

int main() {
    printf("%d", add(3, 4));     // 7
    printf("%d", fib(7));        // 13
    printf("%d", is_prime(7));   // 1
    // while 求和: 1+2+...+10 = 55
}
```

**演示：** `cc /test/09-compiler.c` → 输出 `7 5 13 1 0 55`
**所有输出完全正确** ✅

> 口播（30s）：这个测试程序涵盖函数调用、递归斐波那契、while 循环质数判断。在 shell 中输入 cc 命令，编译器在操作系统内部编译并执行，全部输出正确。这证明了编译器正确处理了函数调用、递归、条件分支和循环四种核心语言特性。

---

## 第 10 页 · ⭐⭐ ELIZA AI

**标题：** 经典 AI 算法 — ELIZA (MIT 1966)

**算法核心：关键词匹配 + 句式变换**

```
"i am tired"     → 匹配 "i am *"    → "Why are you tired?"
"i feel sad"     → 匹配 "i feel *"  → "Why do you feel sad?"
"sorry"          → 匹配 "sorry"     → "No need to apologize..."
"because ..."    → 匹配 "because"   → "Is that the real reason?"
"i love you"     → 匹配 "love"      → "Tell me more about your feelings."
```

**10+ 种情感模式 + 7 种回退回复 + 50 轮对话**

**演示：** Shell 中输入 `chat`，与 AI 进行多轮对话，输入 `bye` 退出。

> 口播（30s）：基于 MIT 1966 年 Weizenbaum 经典算法。关键词匹配识别用户情感意图，句式变换生成回应。支持十余种情感模式，有通用回退回复保证对话不中断。在 Shell 中输入 chat 即可体验。

---

## 第 11 页 · 多用户与状态栏

**标题：** 多用户登录 + 实时状态栏

**左侧 — 多用户系统：**
- 3 用户独立状态（工作目录/历史/终端画面/光标）
- `login User1` / `logout` 切换
- 跨 execve 持久化（/tmp/.current_user）

**右侧 — 实时状态栏：**
```
█████████████████████████████████████████████████████ ← 蓝底
 User1 | Mem 2048/8192K | Procs 5 | Up 01:23:45 | login/out
=========================================================
```

- 每 500ms 刷新，分区着色
- 终端底层保护（scroll/clear 不覆盖状态栏）

> 口播（20s）：多用户系统和实时状态栏都是课程要求之外的功能。每用户数据完全隔离，登录状态跨程序重启保持。状态栏实时展示系统资源，被终端底层保护不受任何操作影响。

---

## 第 12 页 · 界面美化

**标题：** 9 项界面格式化改进

| # | 改进 | 效果 |
|---|------|------|
| 1 | 8 色 ANSI 宏 + sh_error() | 统一红色 `prefix: message` 错误格式 |
| 2 | Banner 框线设计 | `+--+` ASCII 框替代原 "OOOO" 艺术字 |
| 3 | 状态栏分区着色 | Cyan/Green/Yellow 三色数据分区 |
| 4 | help 分类显示 | 5 类青色标题 + 绿色命令名 |
| 5 | ps 状态着色 | 绿=running 黄=runnable 红=zombie |
| 6 | free 可视化内存条 | `[########............] 35%` |
| 7 | ls 多列布局 | 列主序自动列宽 + 目录青色 |
| 8 | uptime 人性化 | X days, X hours, X mins, X secs |
| 9 | HH:MM:SS 运行时间 | 状态栏时间格式 |

**1 个文件，约 200 行改动，不触及终端底层。**

> 口播（20s）：在功能完成的基础上，做了 9 项界面美化。从简单白底黑字变成彩色表格、可视化数据条、统一错误格式。所有改动集中在单个文件中，不影响终端底层。

---

## 第 13 页 · Bug 修复

**标题：** 12 个 Bug 的系统化定位与修复

| # | 现象 | 根因 |
|---|------|------|
| 1 | tree 命令崩溃 | 双重 free() |
| 2 | ps 命令崩溃 | klib 不支持 `%-4d` 格式 |
| 3 | ! 键无法输入 | XOR 翻转 Shift |
| 4 | chat 退格崩溃 | `\b` 渲染非法字形 |
| 5 | edit 文件名错误 | 忽略 argv[1] |
| 6 | ls 不显示目录 | 宏名冲突 + 空初始化 |
| 7 | execve 登录丢失 | 变量重置 |
| 8 | 清屏破坏状态栏 | 从 (0,0) 开始清除 |
| 9 | klib 随机崩溃 | 无效指针过检 |
| 10 | BSS 栈溢出 | 13KB 固定数组 |
| 11 | Shift 状态异常 | XOR 错位 |
| 12 | mm_brk 阻止分配 | 边界过严 |

**跨层分布：** 用户态 8 | 内核 3 | 终端 1

> 口播（20s）：12 个 Bug 跨越用户态、内核和终端三层。最有代表性的是 ps 崩溃——klib 的 printf 不支持格式标志，会将标志字符原样输出并消耗错误的参数。这些修复锻炼了系统化调试能力。

---

## 第 14 页 · 测试程序

**标题：** 10 个 C 测试程序

| # | 测试目标 | # | 测试目标 |
|---|----------|---|----------|
| 01 | 栈变量 + 递归 fib | 06 | 调度算法模拟 |
| 02 | 5 层嵌套调用 | 07 | 21 个 syscall 验证 |
| 03 | 文件数据流 | 08 | ELF magic 验证 |
| 04 | /proc 内容验证 | 09 | 编译器全功能 |
| 05 | 线程 ID + PCB | 10 | 性能基准 |

**运行：** `cc /test/XX-*.c` — 由 Mini C 编译器编译并执行。

> 口播（15s）：10 个测试程序覆盖 OS 全部功能点，可在 Shell 中用 cc 命令编译运行，形成完整的自动化验证闭环。

---

## 第 15 页 · 技术指标

**标题：** 工作量数据

| 维度 | 数值 |
|------|------|
| Shell 命令 | **30+**（课程要求 12） |
| 系统调用 | **22** 个 |
| 自建用户程序 | **3** 个（编译器/AI/编辑器） |
| 编译器 Token / VM 指令 | **30 / 22** |
| C 测试程序 | **10** |
| Bug 修复 | **12** |
| 代码量 | Shell ~1555 行 + 编译器 ~430 行 |
| 跨越语言 | C + C++ + RISC-V Assembly |
| 跨越层级 | 用户程序 → 系统库 → 内核 → 汇编 |

> 口播（15s）：我的工作产出包括 30+ 命令、一个 C 编译器、一个 AI 程序、进程调度器、10 个测试和 12 个 Bug 修复。技术栈跨越 C、C++ 和 RISC-V 汇编，代码覆盖操作系统的全部四层抽象。

---

## 第 16 页 · 现场演示菜单

**标题：** 答辩现场演示命令序列

```
login User1              ← 多用户登录
help                     ← 30+ 命令一览
ls /                     ← 多列彩色列表
tree /                   ← ASCII 目录树
ps                       ← 彩色进程表（状态着色）
free                     ← 可视化内存条
demo                     ← ⭐ 进程调度器（ESC 停止）
cc /test/09-compiler.c   ← ⭐ C 编译器现场编译
cc /test/10-bench.c      ← 性能基准
chat                     ← ⭐ ELIZA AI 对话
  > i am tired
  > sorry
  > bye
uptime                   ← 人性化运行时间
logout                   ← 多用户登出
shutdown                 ← 正常关机
```

> 口播（20s）：这是为答辩优化的演示流程。从登录到命令展示，从调度器到编译器，从 AI 对话到正常关机，整个过程流畅可重复。

---

## 第 17 页 · 总结

**标题：** 收获与展望

**技术成长：**
- 从 RISC-V 汇编到 C 编译器，贯穿 OS 全部四层抽象
- 深入理解 `ecall → trap.S → 调度 → mret` 上下文切换全链路
- 实践编译器完整流水线（词法→语法→代码→VM）
- 学习经典 AI 算法并移植到嵌入式平台

**诚实说明 — 已知待完善：**
- fork() 子进程 Context 恢复 bug（已定位根因：子进程栈缺少父进程栈帧）
- TIME_SHARING 抢占式调度（框架已预留）

> 口播（20s）：这个项目让我在技术深度和工程能力上都有了质的提升。同时也学会了诚实面对不完美——fork 有一个已定位根因的已知 bug，我能在源码中指出并解释修复方向。

---

## 第 18 页 · 致谢

**标题：** 谢谢！欢迎提问

**欢迎深入提问：**
- Mini C 编译器的四阶段实现细节
- fork() Context 拷贝和 bug 分析
- ELIZA 算法的匹配引擎
- trap.S 汇编级上下文切换
- 任何一个 Bug 的定位过程

**现场可演示：** `demo` / `cc` / `chat` / `ps` / `free` / `tree`

---

## 附录：每页时间分配

| 页 | 内容 | 时间 |
|----|------|------|
| 1 | 封面 | 10s |
| 2 | 团队分工 | 15s |
| 3 | 工作全景 | 20s |
| 4 | ELF 加载 | 25s |
| 5 | Shell 命令 | 20s |
| 6 | ⭐ 进程调度 | 50s |
| 7 | ⭐ 上下文切换 | 35s |
| 8 | ⭐⭐ C 编译器 | 55s |
| 9 | ⭐⭐ 编译器证明 | 30s |
| 10 | ⭐⭐ ELIZA AI | 30s |
| 11 | 多用户+状态栏 | 20s |
| 12 | 界面美化 | 20s |
| 13 | Bug 修复 | 20s |
| 14 | 测试程序 | 15s |
| 15 | 技术指标 | 15s |
| 16 | 演示菜单 | 20s |
| 17 | 总结 | 20s |
| 18 | 致谢 | 10s |
| **合计** | | **~8 分 30 秒** |
