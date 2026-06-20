// 05-prime.c — 嵌套循环 + 条件判断：100 以内素数
int is_prime(int n) {
    int d;
    d = 2;
    while (d * d < n + 1) {
        if (n % d == 0) {
            return 0;
        }
        d = d + 1;
    }
    return 1;
}
int main() {
    int n;
    n = 2;
    while (n < 100) {
        if (is_prime(n)) {
            printf("%d", n);
        }
        n = n + 1;
    }
    return 0;
}
