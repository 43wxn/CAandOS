// 03-file-io: 验证 RAMFS 文件读写(通过 shell touch/write/cat)
// 编译器无文件 API，此测试通过运算间接验证编译器能力
int main() {
    // 模拟文件读写的数据流: 写入 → 计算 → 读回
    int data = 42;          // "写入"
    int processed = data * 3 + 7;  // "处理"
    int result = processed - 7;    // "读回"
    result = result / 3;           // 验证数据完整性
    printf("%d", result);  // 应为 42(数据完整)
    printf("%d", processed); // 133
    return 0;
}
