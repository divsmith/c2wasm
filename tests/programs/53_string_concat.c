// EXPECT_EXIT: 42
int main() {
    char *s;

    // Basic adjacent string concatenation
    s = "hello" " " "world";
    if (s[0] != 'h') return 1;
    if (s[5] != ' ') return 2;
    if (s[6] != 'w') return 3;
    if (s[10] != 'd') return 4;
    if (s[11] != 0) return 5;

    // Multi-line concatenation
    s = "foo"
        "bar";
    if (s[0] != 'f') return 6;
    if (s[3] != 'b') return 7;
    if (s[5] != 'r') return 8;
    if (s[6] != 0) return 9;

    // Concatenation with escapes
    s = "abc" "\n" "def";
    if (s[0] != 'a') return 10;
    if (s[3] != 10) return 11;
    if (s[4] != 'd') return 12;
    if (s[7] != 0) return 13;

    return 42;
}
