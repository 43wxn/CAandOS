// 01-meminfo: 验证内存分配器正常工作
// 测试: 多变量分配、大数值计算、深层调用栈
int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    // 大量栈变量(验证栈空间充足)
    int a = 100;
    int b = 200;
    int c = a + b;
    int d = c * 3;
    int e = d / 6;
    printf("%d", a);    // 100
    printf("%d", b);    // 200
    printf("%d", c);    // 300
    printf("%d", d);    // 900
    printf("%d", e);    // 150

    // 深层递归(验证调用栈)
    printf("%d", fib(6));   // 8
    printf("%d", fib(10));  // 55

    return 0;
}
