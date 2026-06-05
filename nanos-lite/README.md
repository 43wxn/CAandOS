# Nanos-lite

Nanos-lite is the simplified version of Nanos (http://cslab.nju.edu.cn/opsystem).
It is ported to the [AM project](https://github.com/NJU-ProjectN/abstract-machine.git).
It is a two-tasking operating system with the following features
* ramdisk device drivers
* ELF program loader
* memory management with paging
* a simple file system
  * with fix number and size of files
  * without directory
  * some device files
* 21 syscall entries in `syscall.h`
  * implemented: exit, yield, open, read, write, getpid, close, lseek, brk,
    fstat, time, execve, unlink, times, gettimeofday, shutdown
  * reserved for future process/thread work: kill, signal, fork, link, wait
* single-process scheduler model with future PCB/thread interfaces reserved
