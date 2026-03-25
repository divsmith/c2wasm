// EXPECT_EXIT: 42
char *greetings[] = {"hello", "world", "foo"};

int main() {
    if (strcmp(greetings[0], "hello") != 0) return 1;
    if (strcmp(greetings[1], "world") != 0) return 2;
    if (strcmp(greetings[2], "foo") != 0) return 3;

    /* test string length */
    if (strlen(greetings[0]) != 5) return 4;
    if (strlen(greetings[1]) != 5) return 5;
    if (strlen(greetings[2]) != 3) return 6;

    return 42;
}
