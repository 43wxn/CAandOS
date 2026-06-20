// 07-fizzbuzz.c — if/else 分支经典题
int main() {
    int i;
    i = 1;
    while (i < 31) {
        if (i % 15 == 0) {
            printf("FizzBuzz");
        }
        else if (i % 3 == 0) {
            printf("Fizz");
        }
        else if (i % 5 == 0) {
            printf("Buzz");
        }
        else {
            printf("%d", i);
        }
        i = i + 1;
    }
    return 0;
}
