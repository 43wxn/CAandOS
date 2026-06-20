# ICS2025 答辩演示流程

## 演示节奏概览（总时长建议 8-12 分钟）

```
启动 → Banner → 登录 → 状态栏 → help → ls → tree → ps → free
    → 文件操作 → C编译器 → AI对话 → 多用户 → 收尾
```

---

## 第一阶段：震撼开场（Boot + Banner）

```
NEMU 启动...
```

**画面出现：**

```
    +----------------------------------------+
    |        DLUT OS  -  nanos-lite          |
    |        Multi-User Terminal v1.0        |
    |                                        |
    |   Users: User1  User2  User3           |
    |   login <username> to start            |
    +----------------------------------------+

login> █
```

> **答辩口播：** "这是基于 RISC-V 32 位架构从零构建的操作系统 nanos-lite。系统启动后进入多用户登录界面，可以看到我们实现了完整的用户隔离——每个用户有独立的工作目录、命令历史和终端状态。"

---

## 第二阶段：登录 + 状态栏

```
login User1
```

**画面变化：**

```
    +----------------------------------------+
    |        Welcome back, User1             |
    +----------------------------------------+

Logged in as User1
User1@nanos-lite:/#
```

**顶部状态栏（每 500ms 实时刷新）：**
```
█████████████████████████████████████████████████████████  ← 蓝底
 User1 | Mem 2048/8192K | Procs 3 | Up 00:01:23 | login/out
=========================================================  ← 等号分隔线
```

> **口播：** "登录后，顶部状态栏实时显示系统资源——内存用量、进程数量、运行时间，采用分区着色设计。这 3 行状态栏在整个使用过程中固定不动，不会被任何操作覆盖。"

```
help
```

**输出：**
```
  --- File Operations ---
  cat <file>         print a file
  touch <file>       create an empty RAMFS file
  write <f> TXT      overwrite a RAMFS file
  ...

  --- System  Info ---
  ps                 show process table
  free               show memory summary
  ...
```

> **口播：** "系统内置 30 余条命令，分为五大类，彩色分类显示。"

---

## 第三阶段：文件系统展示

```
ls /
```

**多列布局输出（目录为青色）：**
```
  bin      dev      etc      home      proc  
  share    test     demos    hello.txt        
```

```
tree /
```

**ASCII 艺术目录树：**
```
/
|-- bin
|   |-- dterm
|   |-- bird
|   |-- pal
|   `-- hello
|-- dev
|   |-- events
|   |-- fb
|   |-- sb
|   `-- sbctl
|-- proc
|   |-- dispinfo
|   |-- files
|   |-- meminfo
|   `-- processes
|-- home
|   `-- welcome.txt
|-- test
|   |-- 01-meminfo.c
|   |-- 02-stack.c
|   |-- ...
|   `-- 10-bench.c
`-- hello.txt
```

> **口播：** "文件系统采用三层架构——静态 ramdisk、动态 RAMFS、以及 /proc 伪文件系统。tree 命令以 ASCII 艺术树展示完整目录结构，目录名以青色高亮。"

---

## 第四阶段：进程与内存

```
ps
```

**彩色进程表：**
```
  PID  PPID  STATE      COMMAND
  ---- ----  ---------  --------------------
     0     0  running    [idle]
     1     0  running    [init]
     2     1  running    /bin/dterm
```

> **口播：** "ps 命令展示进程调度状态——绿色表示正在运行，黄色表示就绪，红色表示僵尸进程。系统实现了 Round-Robin 轮转调度，最多支持 8 个并发进程。"

```
free
```

**可视化内存条：**
```
  Memory: [###########.........................] 35%
  3584K / 8192K total
               total       used       free
  Mem:         8192       3584       4608
```

> **口播：** "free 命令以可视化进度条展示内存使用率。低于 50% 绿色，50-75% 黄色，超过 75% 红色警告。"

---

## 第五阶段：文件操作实战

```
touch /demo.txt
write /demo.txt Hello from nanos-lite OS!
cat /demo.txt
```

> **口播：** "所有文件操作——创建、写入、读取、删除——都通过 RAMFS 动态文件系统实现，支持 128 个文件，每个文件最多 64KB，自动按页增长。"

```
cd /test
ls
cc 01-meminfo.c
```

**编译器输出：**
```
cc: compiling 01-meminfo.c...
fib(6)=8
fib(8)=21
fib(10)=55
...
```

---

## 第六阶段：压轴——Mini C 编译器

```
cc 09-compiler.c
```

**输出：**
```
7
5
13
1
0
55
```

> **口播（重点！）：** "这是本项目最具技术含量的模块之一——**完整的手写 C 编译器**。它运行在操作系统内部，包含：
> - **词法分析器**：识别 30 种 Token，支持注释和预处理指令
> - **递归下降解析器**：处理运算符优先级，支持变量、if/else、while、函数调用和递归
> - **代码生成器**：直接输出字节码，支持可修补跳转实现控制流
> - **栈式虚拟机**：22 条指令，256 深度栈，500 万步运行保护
>
> 09-compiler.c 这个测试文件包含加法函数、斐波那契递归、质数判断、while 循环求和——全部由我们自己的编译器编译并在自己的 VM 上运行！"

```
cc 10-bench.c
```

**输出：**
```
120     ← fact(5)
720     ← fact(6)
55      ← fib(10)
144     ← fib(12)
15      ← 50以内素数个数
25      ← 100以内素数个数
```

---

## 第七阶段：AI 对话演示

```
chat
```

```
 Nano-AI Therapist v1.0
 Based on ELIZA (Weizenbaum, MIT 1966)
 Type 'quit' to exit.

AI: Hello. How are you feeling today?
You: i am tired
AI: Why are you tired?
You: because i worked all day
AI: Is that the real reason?
You: i feel sad
AI: Why do you feel sad?
You: my computer crashed
AI: Do computers worry you?
You: quit
AI: Goodbye! Take care.
```

> **口播：** "我们实现了 MIT 1966 年 Joseph Weizenbaum 的经典 ELIZA 算法——基于关键词匹配和句式变换的 AI 对话程序。它模拟心理治疗师的对话风格，能识别 'I am'、'I feel'、'because'、'sorry' 等十余种情感模式，并给出上下文相关的回应。"

---

## 第八阶段：多用户切换

```
logout
```

**返回登录界面：**
```
    +----------------------------------------+
    |        DLUT OS  -  nanos-lite          |
    |        Multi-User Terminal v1.0        |
    +----------------------------------------+

login> login User2
Logged in as User2
User2@nanos-lite:/#
```

```
history
(空历史——用户隔离)
```

```
cd /home
ls
cat welcome.txt
```

> **口播：** "每个用户的命令历史、工作目录、终端画面完全隔离。用户之间互不可见，实现了基本的多用户安全模型。即使通过 execve 执行外部程序后再返回，登录状态也能自动恢复。"

---

## 收尾展示

```
uptime
```

```
  Uptime: 1 hour, 23 mins, 45 secs
```

```
date
```

```
2026-06-14 21:23:45
```

> **口播：** "以上是我在 ICS2025 操作系统课程中完成的所有工作。从底层的内核定制——包括 22 个系统调用、进程调度、虚拟文件系统、内存管理——到上层的完整 Shell 环境、Mini C 编译器、AI 对话程序和文本编辑器，构建了一个功能齐全、界面美观的 RISC-V 32 位操作系统。谢谢！"

---

## 备用展示（如果时间充裕或有提问）

### 十六进制转储
```
hexdump /test/01-meminfo.c
00000000  2f 2f 20 30 31 2d 6d 65  6d 69 6e 66 6f 2e 63 0a  |// 01-meminfo.c.|
```

### 命令历史
```
history
    1   [  0.000s]  ls /
    2   [  3.250s]  tree /
    3   [  8.100s]  ps
    4   [ 10.450s]  free
    5   [ 15.200s]  chat
```

### 外部程序
```
run bird          # 启动像素鸟游戏
run pal           # 启动仙剑奇侠传
```

---

## 演示 Checklist

| 序号 | 命令 | 展示要点 | 时间 |
|------|------|----------|------|
| 1 | 启动 | 蓝色框线 Banner | 10s |
| 2 | `login User1` | 登录 + 欢迎横幅 | 5s |
| 3 | 状态栏 | 分区着色、实时刷新 | 10s |
| 4 | `help` | 5 类分类彩色帮助 | 15s |
| 5 | `ls /` + `tree /` | 多列彩色、ASCII 树 | 20s |
| 6 | `ps` | 彩色进程状态表 | 10s |
| 7 | `free` | 可视化内存进度条 | 10s |
| 8 | `touch`/`write`/`cat` | 完整文件操作流 | 20s |
| 9 | `cc 09-compiler.c` | ⭐ 编译器核心展示 | 60s |
| 10 | `cc 10-bench.c` | 编译器性能验证 | 15s |
| 11 | `chat` | ⭐ ELIZA AI 对话 | 30s |
| 12 | `logout` → `login User2` | 多用户隔离 | 15s |
| 13 | `uptime` + `date` | 收尾 | 10s |
| — | `history` / `hexdump` | 备用 | — |
