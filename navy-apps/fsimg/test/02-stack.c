// 02-stack: 验证深层调用栈不溢出 + 局部变量隔离
int level5(int x) { return x * 2; }
int level4(int x) { return level5(x + 1); }
int level3(int x) { return level4(x + 1); }
int level2(int x) { return level3(x + 1); }
int level1(int x) { return level2(x + 1); }

int main() {
    int a = 10;
    int r = level1(a);  // 10+1+1+1+1 → 14*2 = 28
    printf("%d", r);    // 28

    // 验证同级变量隔离
    int i = 0;
    int s = 0;
    while (i < 5) {
        int t = i * i;  // 局部变量每次循环重新分配
        s = s + t;
        i = i + 1;
    }
    printf("%d", s);    // 0+1+4+9+16 = 30

    return 0;
}
