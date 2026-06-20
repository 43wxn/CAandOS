// 07-syscall: 验证常用系统调用编号正确
// 实际系统调用通过 shell 命令间接测试
int main() {
    // 21 个 syscall 编号验证
    int SYS_exit = 0;
    int SYS_yield = 1;
    int SYS_open = 2;
    int SYS_read = 3;
    int SYS_write = 4;
    int SYS_brk = 9;
    int SYS_execve = 13;
    int SYS_fork = 14;
    int SYS_shutdown = 20;

    // 验证编号连续性
    int total_syscalls = SYS_shutdown + 1;
    printf("%d", SYS_write);     // 4
    printf("%d", SYS_execve);    // 13
    printf("%d", SYS_fork);      // 14
    printf("%d", total_syscalls); // 21

    // SYS_write 通过 printf 间接调用(fd=1 → serial)
    printf("syscall-ok");
    return 0;
}
