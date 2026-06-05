# Nanos-lite Server Demo on riscv32-NEMU

本项目用于《操作系统课程设计》A 方案：OS 内核实现。路线是在 `riscv32-nemu` 上运行 `nanos-lite`，并把 `/bin/dterm` 改造成类似 Ubuntu Server 登录后的命令行环境。

## 重要定位

当前系统不是完整 Ubuntu，也不是 Linux。

真实 Ubuntu 的链路是：Bootloader 加载 Linux kernel，kernel 挂载磁盘根文件系统，启动 `systemd/init`，再进入登录程序和 shell。Ubuntu 的 `/` 是磁盘分区或镜像中的真实文件系统。

本项目的链路是：NEMU 加载 `nanos-lite` 镜像，`nanos-lite` 初始化设备和 ramdisk，然后直接加载 `/bin/dterm`。这里的 `/bin/dterm` 类似一个简化版 `init + shell`。根目录来自构建时打包进内核镜像的 ramdisk，不是运行时实时挂载宿主机目录。

我们新增了根目录源：

```text
os-root/
```

你可以把想放进 OS 的演示文件放在这里，例如：

```text
os-root/home/welcome.txt
os-root/etc/motd
```

执行 `make ARCH=riscv32-nemu update` 时，`os-root/` 会被复制到 `navy-apps/fsimg/`，再打包进 `ramdisk.img`，最后嵌进 `nanos-lite` 镜像。修改 `os-root/` 后必须重新 `update`，运行中的 OS 不会自动看到宿主机新文件。

如果将来要做到“运行时直接访问宿主机目录”，需要实现 hostfs、磁盘文件系统或 virtio/SD 卡一类设备，这属于扩展目标。

## 启动与调试链路

如果改了 `navy-apps`、`dterm`、`libos`、`libndl`、`libminiSDL`、`os-root` 或默认打包应用：

```bash
cd ~/ics2025/nanos-lite
make ARCH=riscv32-nemu update
make ARCH=riscv32-nemu run
```

如果只改了 `nanos-lite/src/*.c`：

```bash
cd ~/ics2025/nanos-lite
make ARCH=riscv32-nemu run
```

`make run` 不会自动重新编译并打包 `navy-apps`。`make update` 才会重新生成 ramdisk。

当前默认打包应用在 `navy-apps/Makefile`：

```make
APPS = nterm bird pal
TESTS = hello timer-test file-test event-test dummy
```

说明：`APPS = nterm` 这里指源码目录 `navy-apps/apps/nterm`；该目录的 `Makefile` 中 `NAME = dterm`，所以最终安装进 OS 的二进制路径是 `/bin/dterm`。

## 启动到命令行的代码链路

- `nanos-lite/src/main.c`：初始化内存、设备、ramdisk、IRQ、文件系统和进程模块。
- `nanos-lite/src/proc.c`：`init_proc()` 中加载 `/bin/dterm`。
- `nanos-lite/src/loader.c`：解析 ELF 并跳转到用户程序入口。
- `nanos-lite/src/resources.S`：通过 `.incbin "build/ramdisk.img"` 把文件系统镜像嵌进内核。
- `navy-apps/Makefile`：决定哪些应用和根目录文件进入 ramdisk。
- `navy-apps/apps/nterm/src/builtin-sh.cpp`：DTerm server shell 命令实现。

## 当前命令

进入 `root@nanos-lite:/#` 后可演示：

```text
help
pwd
ls
ls /bin
ls /home
cat /home/welcome.txt
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

`Ctrl-C`：在 `pal/bird` 等 SDL 图形应用里按 `Ctrl-C`，应用会退出，内核重新加载 `/bin/dterm` 返回 shell。

`shutdown` / `poweroff`：调用内核关机系统调用，触发 NEMU 退出。

## 输出位置说明

`dterm` 的内建命令输出在图形命令行里。

真正 `execve` 的外部 CLI 程序会替换 `dterm`，它们的 `stdout` 仍然是内核串口，可能输出到宿主机终端。为保证演示效果，`run timer-test`、`run hello`、`run file-test` 做成了 shell 内建演示版，输出留在图形命令行中。`run pal`、`run bird` 仍然启动真实图形应用。

## 六模块分工

课程设计方案给了 6 个技术模块。我们按这 6 个模块组织三人工作，便于体现工作量。

### 成员 A：系统启动 + 中断异常 + 项目主线整合

已完成基础框架，后续继续负责主线稳定性。

对应模块：

- 模块 1：系统启动
- 模块 2：中断与异常处理

目标效果：

- NEMU 能稳定加载 `nanos-lite-riscv32-nemu.bin`。
- 内核完成 `init_mm/init_device/init_ramdisk/init_irq/init_fs/init_proc`。
- `init_proc()` 自动加载 `/bin/dterm` 进入 `root@nanos-lite:/#`。
- `ecall` 能进入内核系统调用分发。
- `shutdown/poweroff` 能触发 NEMU 正常退出。
- `Ctrl-C` 能让 SDL 应用退出并回到 shell。

建议继续完善：

- 整理启动日志，形成清晰 boot trace。
- 给异常路径增加错误提示，例如非法 syscall、ELF 加载失败。
- 在答辩中讲清楚 NEMU、AM、nanos-lite、navy-apps 的层次关系。

### 成员 B：内存管理 + 文件系统

对应模块：

- 模块 3：内存管理
- 模块 5：文件系统

目标效果：

- `new_page()` 提供页级分配。
- `/proc/meminfo` 展示 `MemTotal/MemUsed/MemFree/PageSize/RamfsCap`。
- 运行时 RAMFS 支持 `touch/write/append/cat/rm`。
- `os-root/` 作为宿主机根目录源，`make update` 时被打包进 OS。
- `ls /`、`ls /bin`、`ls /home`、`cat /home/welcome.txt` 展示更像真实系统的目录结构。

建议继续完善：

- 给 RAMFS 增加目录层级解析和更规范的 `stat` 信息。
- 增加 `mkdir/rmdir`。
- 增加 `/proc/cpuinfo`、`/proc/uptime`。
- 给 `meminfo` 做页数统计，而不仅是字节统计。

### 成员 C：用户程序加载 + Shell/应用演示

对应模块：

- 模块 4：进程管理的单进程版本
- 模块 6：用户程序加载与执行

目标效果：

- `loader.c` 能加载 ELF 用户程序。
- `run pal`、`run bird` 能启动图形应用。
- `run timer-test`、`run file-test`、`run hello` 在图形 shell 中有稳定演示输出。
- 未知命令只报错，不会退出 shell。
- `help` 给出完整命令列表。
- 图形应用尺寸尽量匹配 NEMU 窗口。

建议继续完善：

- 给 shell 增加 `history`、`date`、`free`、`hexdump`、`clear`。
- 给外部程序设计“返回 shell”的统一机制。
- 录制 PAL/Bird 演示视频，防止现场机器性能波动。
- 如果时间允许，补一个简化 PCB 状态表，让 `ps` 更像真实 OS。

## 六模块功能点对照

| 课程模块 | 当前可展示内容 | 后续增强 |
|---|---|---|
| 系统启动 | NEMU 启动 nanos-lite，自动进入 dterm | boot trace 文档化 |
| 中断与异常 | `ecall` 系统调用，Ctrl-C 退出应用，shutdown | 更完整异常处理 |
| 内存管理 | 页分配器，`/proc/meminfo` | 页回收、堆统计 |
| 进程管理 | 单进程 exec 模型，`ps` 说明当前模型 | 简化 PCB/状态表 |
| 文件系统 | ramdisk + RAMFS + `/proc` + `os-root` | 目录创建、路径解析增强 |
| 用户程序 | ELF loader，dterm、pal、bird、测试程序 | argv/envp、更多命令 |

## 推荐验收演示顺序

1. `make ARCH=riscv32-nemu update`
2. `make ARCH=riscv32-nemu run`
3. 进入 `root@nanos-lite:/#`
4. `help`
5. `ls`
6. `ls /home`
7. `cat /home/welcome.txt`
8. `meminfo`
9. `write note.txt hello os`
10. `cat note.txt`
11. `run timer-test`
12. `run pal`
13. 在 PAL 中按 `Ctrl-C` 回到 shell
14. `shutdown`

## 常见问题

`ls` 以前看到很多奇怪文件：那是直接展示 ramdisk 全量文件表，包括字体、图片、游戏资源。现在 `ls` 默认展示根目录视角，`/proc/files` 仍保留给调试。

`Ctrl-C` 没反应：确认运行的是重新 `update` 后的镜像；Ctrl-C 要在 NEMU 图形窗口获得焦点时按。

图形应用尺寸不匹配：miniSDL/NDL 已做全屏 canvas 和最近邻缩放，改动后需要重新 `make update`。

PAL 没启动：确认 `APPS` 包含 `pal`，确认 `navy-apps/apps/pal/repo/data` 存在，然后重新 `make update`。

宿主机目录能不能当根目录：可以把 `os-root/` 当作构建时根目录源；不能在当前系统里实时挂载宿主机目录。实时挂载属于后续 hostfs/磁盘文件系统扩展。
