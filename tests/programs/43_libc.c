/* EXPECT_EXIT: 42 */
/* Test libc built-in functions */
int main() {
    char buf[64];
    char *p;
    int r;

    /* ctype */
    if (!isdigit(48)) return 1;
    if (isdigit(65)) return 2;
    if (!isalpha(65)) return 3;
    if (!isalpha(97)) return 4;
    if (!isalnum(48)) return 5;
    if (!isalnum(65)) return 6;
    if (isalnum(32)) return 7;
    if (!isspace(32)) return 8;
    if (!isspace(10)) return 9;
    if (toupper(97) != 65) return 10;
    if (tolower(65) != 97) return 11;

    /* abs */
    if (abs(5) != 5) return 12;
    if (abs(0 - 5) != 5) return 13;

    /* atoi */
    if (atoi("123") != 123) return 14;
    if (atoi("-42") != 0 - 42) return 15;
    if (atoi("  99") != 99) return 16;

    /* strcpy, strcat */
    strcpy(buf, "hello");
    if (strcmp(buf, "hello") != 0) return 17;
    strcat(buf, " world");
    if (strcmp(buf, "hello world") != 0) return 18;

    /* strchr */
    p = strchr("abcdef", 99);
    if (*p != 99) return 19;
    p = strchr("abcdef", 122);
    if (p != (char *)0) return 20;

    /* strncmp */
    if (strncmp("abc", "abd", 2) != 0) return 21;
    if (strncmp("abc", "abd", 3) == 0) return 22;

    /* strstr */
    p = strstr("hello world", "world");
    if (strcmp(p, "world") != 0) return 23;

    /* calloc */
    {
        int *arr;
        arr = (int *)calloc(10, 4);
        if (arr[0] != 0) return 24;
        if (arr[9] != 0) return 25;
        free(arr);
    }

    /* memmove (overlapping) */
    strcpy(buf, "abcdef");
    memmove(buf + 1, buf, 5);
    if (buf[0] != 97) return 26;
    if (buf[1] != 97) return 27;
    if (buf[5] != 101) return 28;

    /* rand/srand */
    srand(42);
    r = rand();
    if (r < 0) return 29;
    if (r > 32767) return 30;

    /* puts */
    puts("test output");

    return 42;
}
