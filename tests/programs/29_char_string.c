// EXPECT_EXIT: 42
/* Test char arrays, string builtins, byte-level ops */
int main() {
    /* strlen */
    int len = strlen("hello");
    if (len != 5) return 1;

    /* strcmp */
    if (strcmp("abc", "abc") != 0) return 2;
    if (strcmp("abc", "abd") >= 0) return 3;
    if (strcmp("abd", "abc") <= 0) return 4;

    /* char pointer reads via byte access */
    char *s = "ABCDE";
    int c0 = *s;
    if (c0 != 65) return 5;

    /* subscript on char pointer — should use byte load */
    int c2 = s[2];
    if (c2 != 67) return 6;

    /* memset + memcmp */
    char *buf = malloc(16);
    memset(buf, 0, 16);
    char *buf2 = malloc(16);
    memset(buf2, 0, 16);
    if (memcmp(buf, buf2, 16) != 0) return 7;

    /* memcpy */
    memcpy(buf, "test", 4);
    if (buf[0] != 116) return 8;
    if (buf[1] != 101) return 9;

    /* strncpy */
    strncpy(buf2, "xyz", 4);
    if (buf2[0] != 120) return 10;
    if (buf2[1] != 121) return 11;
    if (buf2[2] != 122) return 12;

    return 42;
}
