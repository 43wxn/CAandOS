// 05-threads: 验证 proc_create_thread 可创建多个线程
// 通过 demo 命令创建线程后在 shell 中 ps 查看
int main() {
    // 模拟线程 ID 分配
    int base_pid = 10;
    int tid_a = base_pid;
    int tid_b = base_pid + 1;
    int tid_c = base_pid + 2;
    printf("%d", tid_a);   // 10 (logger)
    printf("%d", tid_b);   // 11 (worker)
    printf("%d", tid_c);   // 12 (watchdog)

    // 验证 PCB 数组容量
    int max_proc = 8;      // MAX_NR_PROC
    int used = 5;          // idle + init + 3 threads
    int free = max_proc - used;
    printf("%d", max_proc); // 8
    printf("%d", free);     // 3
    return 0;
}
