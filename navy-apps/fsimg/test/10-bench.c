// 10-bench: 综合性能测试 — 素数 + 斐波那契 + 阶乘
int fact(int n) {
    if (n < 2) return 1;
    return n * fact(n - 1);
}
int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}
int prime_count(int max) {
    int n = 2;
    int cnt = 0;
    while (n < max) {
        int d = 2;
        int ok = 1;
        while (d * d < n + 1) {
            if (n % d == 0) { ok = 0; }
            d = d + 1;
        }
        if (ok) cnt = cnt + 1;
        n = n + 1;
    }
    return cnt;
}

int main() {
    printf("%d", fact(5));        // 120
    printf("%d", fact(6));        // 720
    printf("%d", fib(10));        // 55
    printf("%d", fib(12));        // 144
    printf("%d", prime_count(50)); // 15 (50以内素数个数)
    printf("%d", prime_count(100)); // 25 (100以内素数个数)
    return 0;
}
