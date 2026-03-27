// EXPECT_EXIT: 42
int main() {
    char a;
    char b;
    char f;
    char v;
    char q;

    a = '\a';
    b = '\b';
    f = '\f';
    v = '\v';
    q = '\?';

    if (a != 7) return 1;
    if (b != 8) return 2;
    if (f != 12) return 3;
    if (v != 11) return 4;
    if (q != 63) return 5;

    // Test hex escapes
    {
        char h1;
        char h2;
        h1 = '\x41';
        h2 = '\x0a';
        if (h1 != 65) return 6;
        if (h2 != 10) return 7;
    }

    // Test octal escapes
    {
        char o1;
        char o2;
        char o3;
        o1 = '\101';
        o2 = '\012';
        o3 = '\0';
        if (o1 != 65) return 8;
        if (o2 != 10) return 9;
        if (o3 != 0) return 10;
    }

    // Test in strings
    {
        char *s;
        s = "\a\b\f\v";
        if (s[0] != 7) return 11;
        if (s[1] != 8) return 12;
        if (s[2] != 12) return 13;
        if (s[3] != 11) return 14;

        s = "\x48\x69";
        if (s[0] != 72) return 15;
        if (s[1] != 105) return 16;
    }

    return 42;
}
