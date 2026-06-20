// 04-factorial.c — 函数调用：递归计算阶乘
int fact(int n) {
    if (n < 2) {
        return 1;
    }
    return n * fact(n - 1);
}
int main() {
    int i;
    i = 1;
    while (i < 8) {
        printf("%d", fact(i));
        i = i + 1;
    }
    return 0;
}
