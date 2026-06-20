// 03-fibonacci.c — while 循环计算斐波那契数列
int main() {
    int a;
    int b;
    int c;
    int i;
    a = 0;
    b = 1;
    i = 0;
    printf("%d", a);
    printf("%d", b);
    while (i < 8) {
        c = a + b;
        printf("%d", c);
        a = b;
        b = c;
        i = i + 1;
    }
    return 0;
}
