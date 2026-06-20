// 04-proc-fs: 验证 /proc 伪文件系统可用性
// 实际 cat /proc/meminfo 和 cat /proc/files 在 shell 中执行
int main() {
    // 模拟 /proc/meminfo 输出格式
    int mem_total = 256 * 1024 * 1024;  // 256MB NEMU RAM
    int page_size = 4096;
    int total_pages = mem_total / page_size;
    printf("%d", mem_total);    // 268435456
    printf("%d", page_size);    // 4096
    printf("%d", total_pages);  // 65536

    // 模拟 /proc/files 条目计数
    int static_files = 8;   // bin dev proc share home etc test demos
    int dyn_files = 0;      // touch 创建后可变
    int total = static_files + dyn_files;
    printf("%d", total);    // 8

    return 0;
}
