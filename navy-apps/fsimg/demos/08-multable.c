// 08-multable.c — 嵌套循环：九九乘法表
int main() {
    int i;
    int j;
    i = 1;
    while (i < 10) {
        j = 1;
        while (j < i + 1) {
            printf("%d", i * j);
            j = j + 1;
        }
        i = i + 1;
    }
    return 0;
}
