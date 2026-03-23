// Tests printf with multiple format types
int main() {
    int x = 42;
    int y = 255;
    printf("dec: %d, hex: %x\n", x, y);
    printf("neg: %d\n", -123);
    printf("zero: %d\n", 0);
    return 0;
}
