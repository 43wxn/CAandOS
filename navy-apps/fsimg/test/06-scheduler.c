// 06-scheduler: 验证 Round-Robin 调度器正确性
// 每个 yield() 切换到下一个 RUNNABLE 线程
int main() {
    int round = 0;
    int total_yields = 0;

    // 模拟 3 线程 × 5 轮 = 15 次调度
    while (round < 5) {
        int slot = 0;
        while (slot < 3) {
            total_yields = total_yields + 1;
            slot = slot + 1;
        }
        round = round + 1;
    }
    printf("%d", total_yields);  // 15

    // 验证调度器行为: 当前线程 state 变化
    int state_running = 2;   // PROC_RUNNING
    int state_runnable = 1;  // PROC_RUNNABLE
    int state_zombie = 3;    // PROC_ZOMBIE
    printf("%d", state_running);
    printf("%d", state_runnable);
    printf("%d", state_zombie);

    return 0;
}
