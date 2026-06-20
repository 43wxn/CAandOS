// 06-gcd.c — 辗转相除法求最大公约数
int gcd(int a, int b) {
    while (b != 0) {
        int t;
        t = a % b;
        a = b;
        b = t;
    }
    return a;
}
int main() {
    printf("%d", gcd(48, 18));
    printf("%d", gcd(1071, 462));
    printf("%d", gcd(97, 1));
    return 0;
}
