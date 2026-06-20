// 10-collatz.c — Collatz 猜想：任意正整数最终回到 1
int collatz(int n) {
    int steps;
    steps = 0;
    while (n != 1) {
        if (n % 2 == 0) {
            n = n / 2;
        }
        else {
            n = 3 * n + 1;
        }
        steps = steps + 1;
    }
    return steps;
}
int main() {
    printf("%d", collatz(6));
    printf("%d", collatz(27));
    printf("%d", collatz(97));
    return 0;
}
