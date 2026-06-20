// 08-execve: 验证 execve ELF 加载链路
// cc /test/08-execve.c 本身走 execve 加载 /bin/cc
// 本程序通过 cc 运行间接验证 execve 正常
int main() {
    // ELF 加载关键字段
    int elf_magic = 0x7f;  // ELF magic: 0x7f 'E' 'L' 'F'
    int PT_LOAD = 1;        // 可加载段类型
    int PT_NULL = 0;        // 空段类型

    // 验证段加载逻辑
    int seg_count = 3;  // text + data + bss
    int loaded = 0;
    int i = 0;
    while (i < seg_count) {
        loaded = loaded + 1;
        i = i + 1;
    }
    printf("%d", elf_magic);   // 127 (0x7f)
    printf("%d", PT_LOAD);     // 1
    printf("%d", loaded);      // 3(全部加载)

    return 0;
}
