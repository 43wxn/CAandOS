# Nanos-lite Server Demo on riscv32-NEMU

本项目用于《操作系统课程设计》的 A 方案：OS 内核实现。当前路线是在自研/自改的 `riscv32-nemu` 上运行 `nanos-lite`，并把 `nterm` 改造成类似 Ubuntu Server 登录后的命令行环境。

## 项目定位

我们采用单进程演示模型：

- `NEMU` 是底层 RISC-V32 全系统模拟器。
- `nanos-lite` 是内核，负责启动、设备初始化、系统调用、ELF 加载、ramdisk 文件系统、运行时 RAMFS、`/proc` 信息等。
- `/bin/nterm` 是第一个用户程序，类似 `init + shell`。它提供 `root@nanos-lite:/#` 命令行。
- `run pal`、`run bird` 这类图形程序会通过 `execve` 替换当前 `nterm`。程序退出后，内核重新加载 `/bin/nterm` 回到命令行。

这不是完整 Linux，也不是多进程 Ubuntu。更准确的类比是：我们实现了一个教学 OS 内核和一个 server shell，能够展示启动、系统调用、文件系统、设备、用户程序加载和内存信息。

## 当前演示命令

启动：

```bash
cd ~/ics2025/nanos-lite
make ARCH=riscv32-nemu update
make ARCH=riscv32-nemu run
```

进入图形命令行后可输入：

```text
help
pwd
ls
meminfo
touch note.txt
write note.txt hello nanos-lite
cat note.txt
append note.txt second line
cat note.txt
rm note.txt
uptime
run timer-test
run file-test
run pal
shutdown
```

`Ctrl-C`：在 `pal/bird` 等 SDL 应用中按 `Ctrl-C`，应用会 `exit(130)`，内核随后重新加载 `/bin/nterm` 回到 shell。

`shutdown` / `poweroff`：调用内核 `SYS_shutdown`，让 NEMU 触发 `halt(0)` 退出。

## run 和 update 的区别

`make ARCH=riscv32-nemu run` 不会主动重新编译 `navy-apps`。

如果改了这些内容，需要先 `update`：

- `navy-apps/apps/nterm`
- `navy-apps/libs/libos`
- `navy-apps/libs/libndl`
- `navy-apps/libs/libminiSDL`
- `navy-apps/Makefile` 里的 `APPS` / `TESTS`

推荐链路：

```bash
cd ~/ics2025/nanos-lite
make ARCH=riscv32-nemu update
make ARCH=riscv32-nemu run
```

如果只改了 `nanos-lite/src/*.c`，可以直接：

```bash
cd ~/ics2025/nanos-lite
make ARCH=riscv32-nemu run
```

## 为什么能开机进入命令行

关键代码链路：

- `nanos-lite/src/main.c`：初始化内存、设备、ramdisk、IRQ、文件系统和进程模块。
- `nanos-lite/src/proc.c`：`init_proc()` 中写死加载 `/bin/nterm`。
- `nanos-lite/src/loader.c`：解析 ELF，把 `/bin/nterm` 加载到内存并跳转入口。
- `nanos-lite/src/resources.S`：把 `build/ramdisk.img` 用 `.incbin` 嵌入内核镜像。
- `navy-apps/Makefile`：决定哪些应用被安装进 `fsimg/bin` 并打包进 ramdisk。
- `navy-apps/apps/nterm/src/builtin-sh.cpp`：server shell 的命令实现。

## 默认打包程序

当前 `navy-apps/Makefile` 中默认：

```make
APPS = nterm bird
TESTS = hello timer-test file-test event-test dummy
```

如果要把 PAL 放进新 ramdisk：

```make
APPS = nterm bird pal
```

然后运行：

```bash
cd ~/ics2025/nanos-lite
make ARCH=riscv32-nemu update
make ARCH=riscv32-nemu run
```

进入 shell 后：

```text
run pal
```

注意：PAL 需要对应游戏资源文件；如果 `fsimg/share/games/pal` 资源不存在，程序可能无法完整进入游戏。PAL 也比较重，适合作为 bonus，不建议作为主线唯一演示。

## 输出位置说明

`nterm` 自己的内建命令输出在图形命令行中。

真正 `execve` 的外部 CLI 程序会替换 `nterm`，它们的 `stdout` 仍然是内核串口，所以可能输出到宿主机终端。为保证演示效果，`run timer-test`、`run hello`、`run file-test` 在 shell 中做了内建演示版，输出留在图形命令行里；`run pal`、`run bird` 仍然启动真实图形应用。

## 三人分工

### 成员 A：整体框架与内核主线

当前框架已经由成员 A 搭起。后续负责把项目主线稳定住：

- 维护 `nanos-lite` 启动链路：`main -> init_proc -> naive_uload("/bin/nterm")`。
- 维护系统调用：`open/read/write/lseek/fstat/gettimeofday/execve/unlink/shutdown`。
- 维护运行时 RAMFS、`/proc/files`、`/proc/meminfo`。
- 维护演示脚本，保证 `make update && make run` 能复现。
- 答辩中负责讲清楚“单进程 server shell 模型”和“为什么不是完整 Linux”。

最终效果：系统能稳定启动到 `root@nanos-lite:/#`，支持创建/读写/删除文件、查看内存、运行图形应用、Ctrl-C 返回 shell、关机退出。

### 成员 B：Shell 与用户体验完善

在现有 `nterm` 上继续完善交互体验：

- 优化命令行显示、滚屏、提示符和错误提示。
- 增加更多内建命令，例如 `date`、`free`、`hexdump`、`clear`、`history`。
- 把适合演示的 CLI 程序做成 shell 内建演示版，保证输出在图形命令行中。
- 整理 `help` 输出，让老师现场能直接看到功能点。

最终效果：命令行像简化版 Ubuntu Server，输入不存在命令不会退出，常见命令都有稳定反馈。

### 成员 C：应用、性能与展示材料

负责让演示更顺滑、更好看：

- 维护 `navy-apps/Makefile` 默认打包集合，区分主线应用和 bonus 应用。
- 验证 `run pal`、`run bird`、`run timer-test`、`run file-test`。
- 优化图形刷新、减少无意义调试输出、缩小 ramdisk。
- 准备 PPT、视频、功能点矩阵和演示录屏。

最终效果：主线演示不卡死，PAL/Bird 可作为图形应用亮点，报告中能对应课程设计的启动、系统调用、文件系统、用户程序加载、设备管理等模块。

## 推荐验收演示顺序

1. 展示启动日志，说明 NEMU 加载 `nanos-lite`。
2. 进入 `root@nanos-lite:/#`。
3. 输入 `help` 展示命令列表。
4. 输入 `ls` 展示 ramdisk、设备文件、proc 文件。
5. 输入 `meminfo` 展示页分配和内存统计。
6. 输入 `touch/write/cat/append/rm` 展示运行时 RAMFS。
7. 输入 `run timer-test` 展示定时器系统调用。
8. 输入 `run pal` 或 `run bird` 展示图形应用。
9. 在图形应用中按 `Ctrl-C` 回到 shell。
10. 输入 `shutdown` 退出 NEMU。

## 常见问题

`run` 后没有重新编译应用：先执行 `make ARCH=riscv32-nemu update`。

PAL 没启动：确认 `APPS` 包含 `pal`，确认 PAL 资源文件存在，然后重新 `update`。

输出跑到宿主机终端：这是外部 CLI 程序通过串口 stdout 输出。演示时优先使用 shell 内建命令或已适配的 `run timer-test`。

运行卡顿：默认不要把 PAL/NPlayer/FCEUX 全部打进主线演示；优先演示轻量命令和 `bird`。PAL 作为 bonus。
