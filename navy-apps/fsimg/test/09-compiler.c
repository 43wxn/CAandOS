// 09-compiler: 验证编译器全功能(词法/语法/代码生成/VM)
int add(int a, int b) { return a + b; }
int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}
int is_prime(int n) {
    int d = 2;
    while (d * d < n + 1) {
        if (n % d == 0) return 0;
        d = d + 1;
    }
    return 1;
}

int main() {
    // 词法: 关键字/标识符/数字/字符串/运算符
    // 语法: 变量声明/赋值/if/while/函数调用/return
    // 代码生成: 表达式树 → 字节码
    // VM: 栈式解释执行

    printf("%d", add(3, 4));     // 7
    printf("%d", fib(5));        // 5
    printf("%d", fib(7));        // 13
    printf("%d", is_prime(7));   // 1 (true)
    printf("%d", is_prime(8));   // 0 (false)

    int i = 1;
    int s = 0;
    while (i < 11) {
        s = s + i;
        i = i + 1;
    }
    printf("%d", s);  // 55 (1+2+...+10)

    return 0;
}
