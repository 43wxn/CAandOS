# ics2025 项目结构总览

本项目是**操作系统课程设计三人接力项目**，选题为 **"A、OS 内核实现"**。在 `riscv32-nemu` 虚拟机上运行一个简化操作系统 `nanos-lite`，通过 `navy-apps` 中的 `/bin/dterm` 提供命令行交互环境。

三人接力分工：**A → 系统启动/中断/系统调用** → **B → 内存管理/RAMFS 文件系统** → **C → ELF 加载/Shell/用户程序演示**。

---

## 整体层次架构

```
┌──────────────────────────────────────┐
│  navy-apps/    用户程序 & Shell       │  ← 用户态
│  os-root/      文件系统模板           │
├──────────────────────────────────────┤
│  nanos-lite/   内核                   │  ← 内核态
├──────────────────────────────────────┤
│  abstract-machine/  硬件抽象层 (AM)   │
├──────────────────────────────────────┤
│  nemu/         CPU/设备模拟器         │  ← 硬件层
└──────────────────────────────────────┘
```

编译依赖关系：`nemu/` ← `abstract-machine/` ← `nanos-lite/` ← `navy-apps/`

---

## 各目录详解

### 1. `nemu/` — NJU Emulator（CPU 模拟器）

**作用**：底层全系统模拟器，模拟 RISC-V 32 位 CPU、物理内存、以及 5 类外设（串口、定时器、键盘、VGA、音频）。是操作系统运行的"硬件"。

| 子目录/关键文件 | 用途 |
|---|---|
| `src/nemu-main.c` | 入口：调用 `init_monitor()` 然后 `engine_start()` |
| `src/monitor/monitor.c` | CLI 监控器，解析 `--batch`/`--log` 等参数，加载 OS 镜像 |
| `src/cpu/cpu-exec.c` | CPU 执行循环：`cpu_exec()` → `execute()` → `exec_once()` |
| `src/isa/riscv32/` | RISC-V 32 指令解码执行、中断异常处理（CSR/trap）、MMU |
| `src/device/` | 各设备实现：serial、timer、vga、keyboard、audio、disk、alarm |
| `src/memory/paddr.c` | 物理内存读写，路由 MMIO 设备 |
| `src/monitor/sdb/` | 简易调试器：单步、表达式求值、监视点 |
| `include/` | 自动生成的配置 (`config/auto.conf`)、ISA 头文件、CPU 结构体 |
| `tools/` | 辅助工具：`fixdep`（依赖生成）、`kconfig`（菜单配置）、`qemu-diff` 等差分测试工具 |
| `scripts/build.mk` | 构建规则 |

环境变量：`$NEMU_HOME` 指向此目录。

---

### 2. `abstract-machine/` — 抽象机器层（AM）

**作用**：在 NEMU 和内核之间提供可移植 API，隐藏 ISA/平台差异。定义了 5 类扩展：TRM（图灵机基础）、IOE（I/O）、CTE（上下文/异常）、VME（虚拟内存）、MPE（多处理器）。

| 子目录/关键文件 | 用途 |
|---|---|
| `am/include/am.h` | 核心 API：`putch`、`halt`、`io_read/write`、`cte_init`、`yield`、`kcontext`、`Area`、`Event`、`Context` |
| `am/include/amdev.h` | 设备寄存器定义：`AM_TIMER_UPTIME`、`AM_INPUT_KEYBRD`、`AM_GPU_FBDRAW` 等 |
| `am/src/riscv/nemu/` | RISC-V + NEMU 平台的 CTE 实现：trap 处理、`ecall` 分发、Context 保存恢复 |
| `klib/` | 精简 C 标准库：`printf`、`sprintf`、`memcpy`、`strlen`、`malloc/free`（简单 bump 分配器） |
| `scripts/riscv32-nemu.mk` | 架构特定的编译标志和链接脚本 |
| `scripts/linker.ld` | 链接脚本，定义 `heap.start`/`heap.end` 供内核 mm 使用 |

环境变量：`$AM_HOME` 指向此目录。

---

### 3. `nanos-lite/` — 操作系统内核

**作用**：核心——在 AM 之上运行的微型 OS 内核。实现系统启动、中断处理、系统调用分发、内存管理、RAMFS 文件系统、单进程 ELF 加载。

| 文件 | 用途 |
|---|---|
| `src/main.c` | 内核入口 `main()`：依次初始化 `init_mm` → `init_device` → `init_ramdisk` → `init_irq` → `init_fs` → `init_proc` |
| `src/irq.c` | 事件分发：`EVENT_SYSCALL`、`EVENT_IRQ_TIMER`、`EVENT_YIELD`，通过 `cte_init(do_event)` 注册 |
| `src/syscall.c` | 21 个系统调用分发：`exit/open/read/write/close/lseek/execve/gettimeofday/shutdown` 等 |
| `src/fs.c` | VFS 风格文件系统，三种文件类型：ramdisk 静态文件、RAMFS 动态文件（最多 128 个，单文件 64KB）、设备/进程伪文件 (`/dev/events`、`/dev/fb`、`/proc/meminfo`、`/proc/files`) |
| `src/proc.c` | 单进程模型：`init_proc()` → `proc_execve("/bin/dterm")` → `naive_uload()`。预留 `fork/wait/kill` 接口 |
| `src/loader.c` | ELF 加载器：解析 `Elf32_Ehdr` + `Elf32_Phdr`，加载 `PT_LOAD` 段，清零 `.bss`，跳转到 `e_entry` |
| `src/mm.c` | 物理页分配器：`new_page(nr_page)` 按 4KB 页分配并清零，`free_page()` 部分 LIFO 回收，`get_memory_info()` 统计 |
| `src/ramdisk.c` | `ramdisk_read/write`：直接 `memcpy` 内核内嵌的 ramdisk 镜像区域 |
| `src/device.c` | 串口输出、键盘事件读取（`kd`/`ku` 格式）、显示器信息、framebuffer 写入 |
| `include/*.h` | 接口定义：`proc.h`（PCB 结构）、`fs.h`（文件操作）、`memory.h`（页分配） |

环境变量：依赖 `$AM_HOME` 和 `$NAVY_HOME`。

---

### 4. `navy-apps/` — 用户程序框架

**作用**：用户态的一切——包含所有用户程序、C 库、SDL 风格多媒体库、构建系统。其编译产物被打包成 ramdisk 镜像供 `nanos-lite` 使用。

| 子目录 | 用途 |
|---|---|
| `apps/nterm/` | **DTerm Shell** (`builtin-sh.cpp`)：提供 `help/ls/cat/touch/write/append/rm/meminfo/uptime/ps/run/shutdown` 等命令。Makefile 中 `NAME=dterm`，打包后为 `/bin/dterm` |
| `apps/pal/` | 仙剑奇侠传移植（SDL-PAL），图形演示 |
| `apps/bird/` | Flappy Bird 游戏，图形演示 |
| `apps/nslider/` | NJU 幻灯片播放器 |
| `apps/nwm/` | NJU 窗口管理器 |
| `apps/fceux/` | FC/NES 模拟器移植 |
| `apps/menu/` | 程序启动菜单 |
| `apps/busybox/` | BusyBox 工具集移植 |
| `apps/lua/` | Lua 解释器 |
| `apps/nplayer/` | 媒体播放器 |
| `apps/onscripter/` | ONScripter 视觉小说引擎 |
| `libs/libos/` | 用户态系统调用封装：`_open`/`_read`/`_write` 等 → `ecall` |
| `libs/libc/` | 用户态 C 库 |
| `libs/libam/` | 用户态 AM 绑定 |
| `libs/libminiSDL/` | 简化 SDL 图形库 |
| `libs/libndl/` | NJU 图形库 |
| `libs/libbdf/` | 位图字体库 |
| `libs/libbmp/` | BMP 图像库 |
| `libs/libfixedptc/` | 定点数计算库 |
| `libs/libvorbis/` | Ogg Vorbis 音频解码 |
| `libs/libSDL_image/` `mixer/` `ttf/` | SDL 多媒体扩展 |
| `libs/compiler-rt/` | 编译器运行时库 |
| `tests/` | 测试程序：`hello`、`timer-test`、`file-test`、`bmp-test`、`event-test`、`exec-test` 等 |
| `Makefile` | 主构建文件：编译 `APPS` + `TESTS`，`fsimg` 目标生成 `build/ramdisk.img` + `build/ramdisk.h` |

环境变量：`$NAVY_HOME` 指向此目录。

---

### 5. `os-root/` — 文件系统模板

**作用**：构建时作为根文件系统的"素材源"。里面的文件在 `navy-apps` 构建时被复制到 `fsimg/`，最终打包进 ramdisk 成为 `/home/welcome.txt` 等。

| 内容 | 用途 |
|---|---|
| `home/` | 存放演示用的用户文件（如 `welcome.txt`） |
| `etc/` | 系统配置文件（如 `motd`） |

---

### 6. `am-kernels/` — AM 内核示例

**作用**：独立的 AM 应用程序示例和测试，不依赖 `nanos-lite`，直接在 AM 上运行，用于验证硬件抽象层。

| 子目录 | 用途 |
|---|---|
| `kernels/hello/` | Hello World |
| `kernels/demo/` | 综合演示 |
| `kernels/nemu/` | NEMU 特定演示 |
| `kernels/litenes/` | 精简 FC 模拟器 |
| `kernels/snake/` | 贪吃蛇 |
| `kernels/slider/` | 滑块游戏 |
| `kernels/typing-game/` | 打字游戏 |
| `kernels/bad-apple/` | Bad Apple 视频播放 |
| `kernels/blockchain/` | 区块链演示 |
| `kernels/thread-os/` | 线程 OS 演示 |
| `kernels/yield-os/` | 协程 OS 演示 |
| `benchmarks/` | 性能测试：`coremark`、`dhrystone`、`microbench` |
| `tests/` | CPU/ALU/AM 测试 |

---

### 7. `fceux-am/` — FC/NES 模拟器

**作用**：将 FCEUX（NES 模拟器）移植到 AM 环境。需要将 ROM 放到 `nes/rom/xxx.nes`，支持键盘和图形输出。

| 关键部分 | 用途 |
|---|---|
| `src/` | 模拟器源码 |
| `nes/` | ROM 存放目录 |
| `Makefile` | 构建文件 |

---

### 8. 其他根目录文件

| 文件/目录 | 用途 |
|---|---|
| `init.sh` | 一键设置 `NEMU_HOME`/`AM_HOME`/`NAVY_HOME` 环境变量（`source init.sh`） |
| `.gitignore` | 仅跟踪必要的目录（nemu/*、nanos-lite/*、navy-apps/* 等） |
| `README-team-division.md` | 三人接力分工详情文档（角色分配、接口约定、验收标准、时间线） |
| `progress-report.tex` | LaTeX 课程进度报告模板 |
| `《操作系统课程设计》项目制题目2026.pdf` | 课程要求文件 |
| `member-B-DEMO/` | 成员 B 的演示截图 |
| `zhangyibo-DEMO演示/` | 成员 A（张一波）的演示截图 |

---

## 构建命令速查

```bash
# 1. 设置环境变量（每次新开终端）
source init.sh

# 2. 首次编译或 navy-apps 有改动时，更新 ramdisk
cd nanos-lite
make ARCH=riscv32-nemu update

# 3. 只改内核代码时，直接运行
make ARCH=riscv32-nemu run

# 4. 重新编译 nemu
make -C ../nemu ISA=riscv32
```

三者依赖链：nemu 提供模拟硬件 → abstract-machine 提供可移植 API → nanos-lite 运行内核 → navy-apps 提供用户程序和 ramdisk 镜像。
