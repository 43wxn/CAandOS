// 09-sum.c — 高斯求和：1+2+...+100
int sum_to(int n) {
    int s;
    s = 0;
    while (n > 0) {
        s = s + n;
        n = n - 1;
    }
    return s;
}
int main() {
    printf("%d", sum_to(10));
    printf("%d", sum_to(100));
    return 0;
}
