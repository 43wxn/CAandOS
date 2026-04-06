对南京大学PA项目的学习
主要命令
和nemu相关
1.make clean（nemu根目录执行）
2.make ARCH=riscv32-nemu run（在想要跑的文件夹根目录执行）


和navy apps以及nanos相关
1.编译程序make ISA=riscv32
2.删除build（直接在vscode里找改了哪个文件夹下的代码）
3.在nanos-lite文件夹下
~/ics2025/nanos-lite$ make ARCH=riscv32-nemu update
重新编译nanos-lite,以及导入相关的程序，navy根目录makefile的180可见
要把哪些程序烧到镜像里面


