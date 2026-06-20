# 操作系统课程设计三人接力分工 README

本项目选择《操作系统课程设计》项目制题目 2026 中的 **A、OS 内核实现**。仓库采用 `riscv32-nemu` 作为虚拟机平台，运行 `nanos-lite` 内核，并通过 `navy-apps` 中的 `/bin/dterm` 提供命令行演示环境。

目标不是实现完整 Linux，而是在虚拟机中稳定运行一个简化操作系统，并覆盖课程文件要求的至少 3 个模块、9 个功能点。推荐最终覆盖 5 个模块：系统启动、中断与系统调用、内存管理、文件系统、用户程序加载与执行；进程管理先做简化展示，作为后续加分扩展。

## 一、整体递进关系

三名成员按照 **A -> B -> C** 的顺序接力开发：

| 顺序 | 成员 | 核心职责 | 依赖关系 | 交付给下一位的能力 |
| --- | --- | --- | --- | --- |
| 第一棒 | 成员 A | 系统启动、中断、系统调用、主线集成 | 无，负责底层可运行主线 | 内核能启动、能进入 shell、系统调用可用 |
| 第二棒 | 成员 B | 内存管理、RAMFS 文件系统、`/proc` 信息 | 基于 A 的系统调用和设备初始化 | shell 可通过系统调用读写文件、查看内存 |
| 第三棒 | 成员 C | ELF 加载、用户程序、shell 命令、演示闭环 | 基于 A 的启动/syscall 和 B 的文件系统 | 用户可在 shell 中执行完整验收流程 |

这样安排的原因是：如果 A 没有先完成启动和系统调用，B 的文件读写无法从用户态调用；如果 B 没有完成文件系统和内存统计，C 的 shell 命令和用户程序演示缺少数据来源；C 最后把前两位的底层功能包装成老师能直接操作的系统。

## 二、共同环境与验收基线

三人都使用同一套构建命令：

```bash
cd nanos-lite
make ARCH=riscv32-nemu update
make ARCH=riscv32-nemu run
```

修改 `navy-apps`、`os-root`、`dterm`、用户程序或打包列表后，必须先执行 `make ARCH=riscv32-nemu update`。如果只修改 `nanos-lite/src/*.c`，通常可以直接执行 `make ARCH=riscv32-nemu run`。

命名说明：源码目录仍是 `navy-apps/apps/nterm`，因为 `navy-apps/Makefile` 的 `APPS = nterm` 依赖这个目录名；但该应用的 `Makefile` 设置 `NAME = dterm`，所以最终进入 ramdisk 的用户程序是 `/bin/dterm`。

最终系统应能进入：

```text
root@nanos-lite:/#
```

推荐最终演示命令：

```text
help
ls
ls /bin
ls /home
cat /home/welcome.txt
meminfo
touch note.txt
write note.txt hello nanos-lite
append note.txt second line
cat note.txt
rm note.txt
uptime
run hello
run timer-test
run file-test
run pal
shutdown
```

## 三、成员 A：系统启动、中断、系统调用、主线集成

### 1. 对应课程文件中的功能要求

成员 A 主要对应课程文件中的以下模块：

| 课程模块 | 成员 A 负责的具体功能点 |
| --- | --- |
| 模块 1：系统启动 | 进入内核主函数；初始化内存、设备、ramdisk、中断、文件系统、进程模块；输出启动日志 |
| 模块 2：中断与异常处理 | 注册异常/中断入口；处理 `ecall` 系统调用；处理时钟事件；处理异常路径 |
| 模块 6：用户程序加载与执行的前置能力 | 为后续 `/bin/dterm` 和其他用户程序提供系统调用接口 |

### 2. 主要修改和维护的文件

| 文件 | 作用 |
| --- | --- |
| `nanos-lite/src/main.c` | 内核主函数，负责串联 `init_mm/init_device/init_ramdisk/init_irq/init_fs/init_proc` |
| `nanos-lite/src/irq.c` | 事件分发，处理 `EVENT_SYSCALL`、`EVENT_IRQ_TIMER`、`EVENT_YIELD` |
| `nanos-lite/src/syscall.c` | 系统调用分发，支持 `open/read/write/lseek/close/gettimeofday/execve/exit/shutdown` 等 |
| `nanos-lite/src/device.c` | 初始化串口、键盘、VGA、音频等设备，为 shell 和图形程序提供设备基础 |
| `nanos-lite/src/ramdisk.c` | 初始化并读写 ramdisk，保证内核可以访问打包进镜像的文件 |
| `nanos-lite/src/proc.c` | 第一阶段负责启动 `/bin/dterm`，保证系统进入 shell |
| `nanos-lite/include/proc.h`、`nanos-lite/include/fs.h` | 维护跨模块接口，避免 B、C 后续调用混乱 |

### 3. 具体实现任务

1. 保证 `main()` 的初始化顺序稳定：先内存，再设备，再 ramdisk，再中断，再文件系统，最后进入进程/用户程序。
2. 在启动过程中输出清晰日志，例如 build time、内存起始位置、设备初始化完成、文件系统初始化完成、进入 `/bin/dterm`。
3. 在 `irq.c` 中保证 `EVENT_SYSCALL` 能正确进入 `do_syscall()`，`EVENT_YIELD` 和 `EVENT_IRQ_TIMER` 不导致死机。
4. 在 `syscall.c` 中实现或确认以下系统调用可用：
   - `SYS_open`
   - `SYS_read`
   - `SYS_write`
   - `SYS_lseek`
   - `SYS_close`
   - `SYS_gettimeofday`
   - `SYS_execve`
   - `SYS_exit`
   - `SYS_shutdown`
5. 为非法 syscall 和异常路径增加可读日志，避免答辩演示时只看到无意义崩溃。
6. 维护 `SYS_exit` 的行为：用户程序退出后重新加载 `/bin/dterm`，保证可以回到 shell。
7. 维护 `SYS_shutdown`：shell 中输入 `shutdown` 或 `poweroff` 后 NEMU 能正常退出。
8. 和成员 B 约定文件系统接口：`fs_open/fs_read/fs_write/fs_lseek/fs_close/fs_unlink/fs_fstat` 的参数和返回值。
9. 和成员 C 约定程序执行接口：`execve(path, argv, envp)` 暂时可以先只使用 `path`，后续再扩展参数传递。

### 4. 当前已完成演示流程

这部分是成员 A 当前已经搭好的可运行演示闭环，后续成员 B/C 是在这个基础上继续增强文件系统、内存统计、shell 命令和用户程序展示。

启动命令：

```bash
cd ~/ics2025/nanos-lite
make ARCH=riscv32-nemu update
make ARCH=riscv32-nemu run
```

进入 `root@nanos-lite:/#` 后依次输入：

```text
help
ls /bin
meminfo
touch demo.txt
write demo.txt hello os course
cat demo.txt
append demo.txt second line
cat demo.txt
rm demo.txt
run pal
Ctrl-C
ps
shutdown
```

当前这条流程能够展示：

| 演示动作 | 对应成员 A 已完成的底层能力 | 后续交给队友继续增强的方向 |
| --- | --- | --- |
| 启动进入 dterm | `main()` 初始化顺序、`init_proc()`、`proc_execve("/bin/dterm")`、`naive_uload()` | 成员 C 继续扩展 shell 体验 |
| `help` / `ls /bin` | ramdisk 已能打包并加载 `/bin/dterm`、`/bin/pal` 等用户程序 | 成员 C 维护更多命令和应用列表 |
| `meminfo` | `open/read` syscall 链路已经打通，能读取 `/proc/meminfo` | 成员 B 继续完善内存统计字段 |
| 创建/写入/读取/追加/删除文件 | `open/read/write/unlink` 已经能从用户态进入 `syscall.c` 并转到 `fs.c` | 成员 B 继续增强 RAMFS、目录和回收 |
| `run pal` | `execve` 已能从用户态进入内核，并通过 `naive_uload()` 加载 PAL ELF | 成员 C 继续完善用户程序参数和演示稳定性 |
| `Ctrl-C` 回 shell | SDL 应用退出后 `SYS_exit` 重新加载 `/bin/dterm` | 成员 C 继续完善外部程序返回机制 |
| `ps` | 当前单进程模型和未来进程接口已经预留 | 后续可扩展 PCB 表和调度展示 |
| `shutdown` | `SYS_shutdown` 已能触发 `halt()` 正常退出 NEMU | 后续可补关机前资源同步 |

队友快速理解时，先记住三条链路：

```text
启动链路：
NEMU -> nanos-lite main() -> init_proc() -> proc_execve("/bin/dterm") -> naive_uload()
```

```text
文件链路：
dterm 命令 -> libos open/read/write/unlink -> ecall -> syscall.c -> fs.c -> ramdisk/RAMFS/proc/dev
```

```text
运行程序链路：
dterm run pal -> execve("/bin/pal") -> SYS_execve -> proc_execve() -> naive_uload() -> PAL
```

### 5. 当前演示截图素材

成员 A 当前演示截图已经统一放到：

```text
zhangyibo-DEMO演示/
```

建议在 PPT 或答辩材料中按下面顺序使用：

| 截图文件 | 建议展示内容 |
| --- | --- |
| [1.png](zhangyibo-DEMO演示/1.png) | 系统启动进入 dterm，说明 NEMU -> nanos-lite -> `/bin/dterm` 链路 |
| [2.png](zhangyibo-DEMO演示/2.png) | 命令行基础操作，例如 `help`、`ls /bin`、系统可见程序 |
| [4.png](zhangyibo-DEMO演示/4.png) | 文件系统或内存演示，例如创建文件、读写文件、`meminfo` |
| [PAL.png](zhangyibo-DEMO演示/PAL.png) | `run pal` 图形程序演示，说明 `execve`、ELF loader 和 `/dev/fb` |

这些截图属于成员 A 当前已完成的演示闭环材料，后续成员 B/C 可以继续补充更完整的文件系统、内存统计、shell 命令和用户程序截图。

### 6. 系统调用交接表

`navy-apps/libs/libos/src/syscall.h` 中目前定义了 21 个系统调用号。成员 A 需要保证 `nanos-lite/src/syscall.c` 中每个号都有明确分支：能实现的走真实功能，暂时不能实现的返回 `-1` 并留下后续扩展接口。

| 编号 | syscall | 当前状态 | 当前实现/返回 | 后续扩展点 |
| --- | --- | --- | --- | --- |
| 0 | `SYS_exit` | 已实现 | 记录退出码并重新加载 `/bin/dterm` | 多进程时改为标记 ZOMBIE 并调度其他进程 |
| 1 | `SYS_yield` | 已实现 | 返回 0 并进入 `yield()` | 时间片轮转时接入 ready queue |
| 2 | `SYS_open` | 已实现 | `fs_open(path, flags, mode)` | 真实目录/inode/权限 |
| 3 | `SYS_read` | 已实现 | `fs_read(fd, buf, count)` | 每次 open 独立 fd 表 |
| 4 | `SYS_write` | 已实现 | `fs_write(fd, buf, count)` | 设备异步写、管道 |
| 5 | `SYS_kill` | 部分实现 | 支持 `kill(getpid(), 0)` 探测 | 信号递送、进程终止 |
| 6 | `SYS_getpid` | 已实现 | 单进程返回 1 | 多进程时返回当前 PCB pid |
| 7 | `SYS_close` | 已实现 | `fs_close(fd)` | 打开文件表引用计数 |
| 8 | `SYS_lseek` | 已实现 | `fs_lseek(fd, offset, whence)` | 目录/特殊设备 seek 策略 |
| 9 | `SYS_brk` | 简化实现 | 返回 0，用户态 libos 维护堆顶 | 真正维护进程 brk 和页映射 |
| 10 | `SYS_fstat` | 已实现 | `fs_fstat(fd, statbuf)` | 更完整 `stat` 字段 |
| 11 | `SYS_time` | 已实现 | 返回开机秒数 | 接入 RTC/真实日期 |
| 12 | `SYS_signal` | 预留 | 返回 -1 | PCB 信号处理表 |
| 13 | `SYS_execve` | 已实现 | `proc_execve(path, argv, envp)`，内部加载 ELF | argv/envp 入栈、地址空间切换 |
| 14 | `SYS_fork` | 预留 | 返回 -1 | 复制 PCB、Context、地址空间 |
| 15 | `SYS_link` | 预留 | 返回 -1 | inode 和硬链接引用计数 |
| 16 | `SYS_unlink` | 已实现 | `fs_unlink(path)`，只删除 RAMFS 文件 | 删除目录项并回收 inode/page |
| 17 | `SYS_wait` | 预留 | 返回 -1，可写最近退出码用于调试 | 等待并回收子进程 |
| 18 | `SYS_times` | 已实现 | 返回 uptime ticks，填充简化 `struct tms` | 精确用户态/内核态 CPU 计时 |
| 19 | `SYS_gettimeofday` | 已实现 | 返回 AM uptime 秒/微秒 | RTC/时区 |
| 20 | `SYS_shutdown` | 已实现 | `halt(status)` 退出 NEMU | 关机前 sync/保存状态 |

用户态到内核态的参数约定：

```text
a7 = syscall 编号
a0 = 第 1 个参数，也用于返回值
a1 = 第 2 个参数
a2 = 第 3 个参数
```

例如 `execve("/bin/pal", argv, envp)` 进入内核时是：

```text
a7 = SYS_execve
a0 = "/bin/pal" 字符串在用户内存中的地址
a1 = argv 指针
a2 = envp 指针
```

当前 `proc_execve()` 只使用 `path`，`argv/envp` 已经作为接口保留，后续成员 C 可以继续实现参数入栈。

### 7. 未来多任务/线程调度接口

当前项目为了演示稳定性，仍采用单进程模型：外部程序通过 `execve` 替换 `/bin/dterm`，程序退出后 `SYS_exit` 重新加载 `/bin/dterm`。但成员 A 已经在 `nanos-lite/include/proc.h` 和 `nanos-lite/src/proc.c` 中预留了后续多任务接口：

| 接口 | 当前行为 | 未来实现方向 |
| --- | --- | --- |
| `proc_getpid()` | 返回 `1` | 返回当前 PCB 的 pid |
| `proc_execve(path, argv, envp)` | 加载 ELF 并跳转 | 替换当前进程地址空间并设置用户栈参数 |
| `proc_exit_current(status)` | 重新加载 `/bin/dterm` | 当前进程变为 ZOMBIE，再调度下一个进程 |
| `proc_fork()` | 返回 `-1` | 复制父进程 PCB/Context/地址空间 |
| `proc_wait(status)` | 返回 `-1` | 阻塞等待子进程退出并回收 |
| `proc_kill(pid, sig)` | 仅支持 `kill(getpid(), 0)` | 发送信号或终止目标进程 |
| `proc_create_thread(name, entry, arg)` | 返回 `-1` | 分配栈、构造 Context、加入 runnable 队列 |
| `proc_schedule(prev)` | 返回当前 Context | 从 runnable 队列选择下一个 Context |

未来真正做时间片轮转时，建议先完成三步：

1. 给 `PCB` 增加独立内核栈、用户栈和 `Context` 初始化。
2. 维护 `PROC_RUNNABLE/PROC_RUNNING/PROC_ZOMBIE` 队列。
3. 在 `irq.c` 的 `EVENT_IRQ_TIMER` 和 `SYS_yield` 中调用新的 `proc_schedule()`。

### 8. 交付标准

成员 A 完成后，成员 B 和 C 应能基于它继续开发。A 的验收标准：

```text
make ARCH=riscv32-nemu update
make ARCH=riscv32-nemu run
```

系统能够稳定进入：

```text
root@nanos-lite:/#
```

并且以下命令或行为可用：

```text
help
uptime
shutdown
```

其中 `uptime` 依赖 `gettimeofday` 系统调用，`shutdown` 依赖 `SYS_shutdown`。

### 9. 交接给成员 B 的内容

成员 A 交接时应说明：

1. 当前可用的 syscall 列表。
2. 文件系统相关 syscall 如何调用到 `fs.c`。
3. `gettimeofday` 是否可稳定返回时间。
4. `shutdown` 是否可稳定退出 NEMU。
5. 如果遇到 panic，日志从哪里看。

## 四、成员 B：内存管理、RAMFS 文件系统、`/proc` 信息

### 1. 对应课程文件中的功能要求

成员 B 主要对应课程文件中的以下模块：

| 课程模块 | 成员 B 负责的具体功能点 |
| --- | --- |
| 模块 3：内存管理 | 物理页分配；页大小 4KB；内存使用统计；释放/复用作为扩展 |
| 模块 5：文件系统 | RAMFS；文件创建、删除、打开、关闭、读写；seek；文件描述符管理 |
| 模块 6：用户程序加载与执行的支撑 | 为 shell 和用户程序提供 `/bin`、`/home`、`/proc` 等可访问文件 |

### 2. 主要修改和维护的文件

| 文件 | 作用 |
| --- | --- |
| `nanos-lite/src/mm.c` | 页级内存分配，维护 `new_page()`、`free_page()`、`get_memory_info()` |
| `nanos-lite/include/memory.h` | 内存管理接口声明 |
| `nanos-lite/src/fs.c` | 文件表、RAMFS、设备文件、`/proc/meminfo`、`/proc/files` |
| `nanos-lite/include/fs.h` | 文件系统接口声明 |
| `nanos-lite/src/ramdisk.c` | ramdisk 读写支撑 |
| `navy-apps/Makefile` | 控制哪些应用和 `os-root/` 内容被打包进 ramdisk |
| `os-root/` | 作为构建时根目录来源，放置 `/home/welcome.txt`、`/etc/motd` 等演示文件 |

### 3. 具体实现任务

1. 保证 `new_page(nr_page)` 按 4KB 页分配内存，并清零返回的页。
2. 在 `get_memory_info()` 中统计：
   - `MemTotal`
   - `MemUsed`
   - `MemFree`
   - `PageSize`
   - RAMFS 已占用容量
3. 扩展 RAMFS 容量，建议将动态文件数量从当前的 `32` 提升到课程要求的至少 `128`。
4. 保证单文件最大大小至少达到课程要求中的 `64KB`。
5. 实现并测试动态文件创建：
   - `touch file`
   - `write file text`
   - `append file text`
6. 实现并测试动态文件读取和删除：
   - `cat file`
   - `rm file`
7. 保证 `fs_lseek()` 支持：
   - `SEEK_SET`
   - `SEEK_CUR`
   - `SEEK_END`
8. 维护 `/proc/meminfo`，让 shell 的 `meminfo` 命令能展示内核内存状态。
9. 维护 `/proc/files`，让 shell 的 `ls` 能根据文件表展示目录内容。
10. 支持基本目录视图：
    - `/`
    - `/bin`
    - `/dev`
    - `/proc`
    - `/home`
11. 维护 `os-root/` 到 `navy-apps/fsimg/` 的打包流程，让 `/home/welcome.txt` 这类文件能随镜像进入系统。
12. 如果时间允许，实现 `free_page()` 的简单页回收，或至少在文档中说明当前内存分配是 bump allocator，RAMFS 删除文件只释放文件表项，不回收物理页。

### 4. 交付标准

成员 B 完成后，成员 C 应能直接在 shell 上使用文件系统功能。B 的验收命令：

```text
meminfo
ls
ls /proc
cat /proc/meminfo
touch note.txt
write note.txt hello os
append note.txt second line
cat note.txt
rm note.txt
cat note.txt
```

预期表现：

1. `meminfo` 能显示内存总量、已用、空闲、页大小。
2. `touch/write/append/cat/rm` 不导致内核崩溃。
3. 删除后的文件再次 `cat` 应提示不存在。
4. `ls /home` 能看到从 `os-root/home/` 打包进去的文件。

### 5. 交接给成员 C 的内容

成员 B 交接时应说明：

1. 哪些目录是“视图模拟”，哪些是真正来自 ramdisk。
2. 哪些文件可以写，哪些文件只读。
3. RAMFS 最大文件数量和单文件最大大小。
4. `/proc/files` 的输出格式，方便 C 在 shell 中解析。
5. `meminfo` 中各字段的含义，方便 C 写演示说明。

## 五、成员 C：ELF 加载、用户程序、shell 命令、演示闭环

### 1. 对应课程文件中的功能要求

成员 C 主要对应课程文件中的以下模块：

| 课程模块 | 成员 C 负责的具体功能点 |
| --- | --- |
| 模块 4：进程管理 | 先实现单进程 `execve` 演示模型；`ps` 展示当前进程模型；多进程调度作为扩展 |
| 模块 6：用户程序加载与执行 | ELF 加载；shell 命令；运行至少 5 个用户程序或演示命令 |
| 模块 5：文件系统的用户态演示 | 将 B 的文件系统能力包装成 `ls/cat/touch/write/append/rm` 命令 |

### 2. 主要修改和维护的文件

| 文件 | 作用 |
| --- | --- |
| `nanos-lite/src/loader.c` | 解析 ELF，将用户程序加载到内存并跳转执行 |
| `nanos-lite/src/proc.c` | 启动 `/bin/dterm`，后续可扩展 PCB 和进程状态 |
| `nanos-lite/include/proc.h` | 进程和加载相关接口 |
| `navy-apps/apps/nterm/src/builtin-sh.cpp` | DTerm shell 内建命令，是最终演示的主要界面 |
| `navy-apps/Makefile` | 维护 `APPS` 和 `TESTS` 打包列表 |
| `navy-apps/tests/hello/main.c` | 用户程序演示：打印 hello |
| `navy-apps/tests/timer-test/main.c` | 用户程序演示：时间系统调用 |
| `navy-apps/tests/file-test/main.c` | 用户程序演示：文件系统调用 |
| `navy-apps/apps/pal/Makefile`、`navy-apps/apps/bird/Makefile` | 图形应用演示 |

### 3. 具体实现任务

1. 保证 `loader.c` 能识别 ELF 文件头，并加载所有 `PT_LOAD` 段。
2. 保证加载程序时正确处理：
   - `p_offset`
   - `p_vaddr`
   - `p_filesz`
   - `p_memsz`
   - `e_entry`
3. 保证 `/bin/dterm` 可以作为系统启动后的默认用户程序。
4. 在 shell 中维护基础命令：
   - `help`
   - `clear`
   - `pwd`
   - `ls`
   - `cat`
   - `echo`
   - `meminfo`
   - `uptime`
   - `shutdown`
5. 在 shell 中维护文件命令：
   - `touch`
   - `write`
   - `append`
   - `rm`
6. 在 shell 中维护程序执行命令：
   - `run hello`
   - `run timer-test`
   - `run file-test`
   - `run pal`
   - `run bird`
7. 保证未知命令只报错，不退出 shell。
8. 实现或维护 `ps` 命令，说明当前是单进程 `execve` 演示模型：
   - `/bin/dterm` 是默认 shell
   - 外部程序通过 `execve` 替换当前程序
   - 程序退出后由内核重新加载 `/bin/dterm`
9. 维护 `navy-apps/Makefile` 中的打包列表，保证至少 5 个用户程序或演示程序进入 `/bin`。
10. 对图形程序进行演示测试，重点确认 `pal` 或 `bird` 能启动，并能通过 `Ctrl-C` 或退出机制回到 shell。
11. 整理最终演示脚本、答辩截图、视频录制流程。
12. 如果时间允许，扩展：
    - `history`
    - `date`
    - `free`
    - `hexdump`
    - 简化 PCB 表
    - 简化 RR 调度演示

### 4. 交付标准

成员 C 完成后，系统应具备完整演示闭环。C 的验收命令：

```text
help
ls
ls /bin
ls /home
cat /home/welcome.txt
echo hello
meminfo
uptime
ps
run hello
run timer-test
run file-test
run pal
shutdown
```

预期表现：

1. 每条命令都有清晰输出。
2. 未知命令不会导致 shell 退出。
3. 至少 5 个用户程序或演示命令可运行。
4. 图形程序能展示系统具备设备和用户态程序运行能力。
5. `shutdown` 能正常结束虚拟机。

### 5. 交接给全组的内容

成员 C 最后交付：

1. 最终演示命令顺序。
2. 每个命令对应的课程功能点。
3. 演示截图或视频素材。
4. 答辩时对“为什么不是完整 Linux”的解释：
   - 本项目是简化 OS 内核。
   - NEMU 是虚拟机。
   - `nanos-lite` 是内核。
   - `dterm` 是用户态 shell。
   - `ramdisk` 是构建时打包的简化文件系统。

## 六、三人工作量平衡

| 成员 | 主要模块数量 | 主要文件数量 | 主要功能数量 | 工作特点 |
| --- | --- | --- | --- | --- |
| 成员 A | 2 个核心模块 + 集成 | 7 个左右 | 9 个 syscall/启动功能 | 底层风险高，负责全项目可运行基线 |
| 成员 B | 2 个核心模块 | 7 个左右 | 10 个内存/文件功能 | 实现量大，功能最容易被现场操作验证 |
| 成员 C | 2 个展示模块 + 文档演示 | 8 个左右 | 12 个 shell/程序功能 | 用户可见度最高，负责最终答辩闭环 |

三个人工作量基本接近：A 负责底层稳定性，B 负责资源管理和文件系统，C 负责用户程序和最终展示。A 的工作决定系统能否运行，B 的工作决定系统是否像 OS，C 的工作决定老师能否直观看到功能。

## 七、课程要求与本项目功能点对照

| 课程要求模块 | 本项目建议实现功能点 | 主要负责人 |
| --- | --- | --- |
| 系统启动 | NEMU 启动 `nanos-lite`；进入 `main()`；初始化各子系统；输出启动日志 | A |
| 中断与异常处理 | `ecall` 系统调用；时钟事件；异常日志；`shutdown` | A |
| 内存管理 | 4KB 页分配；内存统计；`/proc/meminfo`；RAMFS 容量统计 | B |
| 进程管理 | 单进程 `execve` 模型；`ps` 展示；退出后回到 shell | C |
| 文件系统 | ramdisk；RAMFS；open/read/write/lseek/close/unlink；`/proc/files` | B |
| 用户程序加载与执行 | ELF loader；`dterm` shell；运行测试程序和图形程序 | C |

按这个对照表，项目至少可以稳定展示以下 12 个功能点：

1. 虚拟机启动并进入内核。
2. 内核初始化多个子系统。
3. 系统调用从用户态进入内核态。
4. 获取系统运行时间。
5. 正常关机退出 NEMU。
6. 页级内存分配。
7. 内存使用统计。
8. RAMFS 文件创建。
9. RAMFS 文件读写追加。
10. RAMFS 文件删除。
11. ELF 用户程序加载。
12. shell 执行命令和用户程序。

## 八、推荐开发时间线

### 第 1 阶段：成员 A 完成底层主线

目标：系统能稳定启动并进入 shell。

验收：

```text
help
uptime
shutdown
```

### 第 2 阶段：成员 B 完成内存和文件系统

目标：shell 可以调用内核文件系统和内存统计。

验收：

```text
meminfo
touch note.txt
write note.txt hello
append note.txt world
cat note.txt
rm note.txt
```

### 第 3 阶段：成员 C 完成 shell 和用户程序演示

目标：形成完整用户可操作系统。

验收：

```text
ls /bin
run hello
run timer-test
run file-test
run pal
ps
```

### 第 4 阶段：三人联合收尾

目标：修复崩溃点，统一文档、PPT、视频、答辩口径。

联合验收：

```text
make ARCH=riscv32-nemu update
make ARCH=riscv32-nemu run
```

按最终演示命令完整跑一遍，确保每条命令可重复执行。

## 九、风险控制与加分方向

### 必须控制的风险

1. 不要一开始就实现完整 `fork/wait/RR`，这会影响主线稳定性。
2. 不要把本项目描述成 Ubuntu 或 Linux，它是简化 OS 内核。
3. 修改 `navy-apps` 或 `os-root` 后必须重新 `make update`。
4. 图形程序现场可能受机器性能影响，建议提前录制视频作为备份。
5. 文件系统目前以 ramdisk 和 RAMFS 为主，不是运行时挂载宿主机目录。

### 时间充足时的加分方向

1. 成员 A：增加更完整的异常日志和 boot trace。
2. 成员 B：实现简单 `free_page()` 或 RAMFS 页回收。
3. 成员 B：增加 `mkdir/rmdir` 和更真实的目录层级。
4. 成员 C：增加 `history/date/free/hexdump`。
5. 成员 C：增加简化 PCB 表，让 `ps` 展示更多状态。
6. 全组：补充系统结构图和启动流程图，用于答辩 PPT。

## 十、最终答辩表述建议

答辩时可以这样介绍：

本项目实现了一个运行在 `riscv32-nemu` 虚拟机上的简化操作系统。系统启动后进入 `nanos-lite` 内核，内核完成内存、设备、ramdisk、中断、文件系统和用户程序初始化，然后加载 `/bin/dterm` 作为命令行 shell。用户可以在 shell 中执行文件读写、内存查看、运行用户程序、启动图形应用和关机等操作。项目覆盖了课程要求中的系统启动、中断与系统调用、内存管理、文件系统、用户程序加载与执行等模块。

## 十一、成员 A 交接说明

### 1. 当前启动链路

```text
NEMU
  -> 加载 nanos-lite 镜像
  -> nanos-lite/src/main.c
  -> init_mm()
  -> init_device()
  -> init_ramdisk()
  -> init_irq()
  -> init_fs()
  -> init_proc()
  -> proc_execve("/bin/dterm", NULL, NULL)
  -> naive_uload(NULL, "/bin/dterm")
  -> loader.c 解析 ELF 并跳到 dterm 入口
```

`/bin/dterm` 不是内核的一部分，而是 `navy-apps/apps/nterm` 编译出来的用户态 ELF 程序。源码目录仍叫 `nterm`，但应用 `Makefile` 里设置了 `NAME = dterm`，因此最终安装到 ramdisk 的路径是 `/bin/dterm`。

### 2. shell 运行程序链路

在 dterm 里输入：

```text
run pal
```

调用链是：

```text
dterm 内建 shell
  -> execve("/bin/pal", argv, envp)
  -> libos 把 SYS_execve 放入 a7，把 path/argv/envp 放入 a0/a1/a2
  -> ecall
  -> AM 根据 mcause 识别 EVENT_SYSCALL
  -> nanos-lite/src/irq.c 调用 do_syscall()
  -> nanos-lite/src/syscall.c 的 SYS_execve 分支
  -> proc_execve("/bin/pal", argv, envp)
  -> naive_uload(NULL, "/bin/pal")
  -> loader.c 加载 PAL ELF 并跳转
```

当前 `execve` 是单进程替换模型：PAL 会替换 dterm。PAL 退出或按 `Ctrl-C` 后，`SYS_exit` 重新加载 `/bin/dterm`，于是回到 shell。

### 3. 成员 A 交接 checklist

成员 A 交给成员 B/C 前，至少确认：

```bash
cd ~/ics2025/nanos-lite
make ARCH=riscv32-nemu update
make ARCH=riscv32-nemu run
```

进入 shell 后执行：

```text
help
uptime
ps
run hello
shutdown
```

预期：

1. `help` 能显示 dterm 内建命令。
2. `uptime` 能通过 `SYS_gettimeofday` 显示运行时间。
3. `ps` 能说明当前是单进程 `/bin/dterm` 模型。
4. `run hello` 不会破坏 shell 演示。
5. `shutdown` 能正常退出 NEMU。

如果只改了 `nanos-lite/src/*.c`，可以先跑：

```bash
make ARCH=riscv32-nemu image
```

如果改了 `navy-apps/apps/nterm`、`navy-apps/libs/libos`、`os-root` 或应用打包列表，必须重新：

```bash
make ARCH=riscv32-nemu update
```
