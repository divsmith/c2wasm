// Adjacent string literal concatenation
int main() {
    char *greeting;
    char *multi;

    greeting = "Hello" ", " "World!";
    printf("%s\n", greeting);

    // Multi-line string concatenation
    multi = "This is a "
            "long string "
            "split across lines.";
    printf("%s\n", multi);

    // With escape sequences
    printf("Bell: \\a = %d\n", '\a');
    printf("Backspace: \\b = %d\n", '\b');
    printf("Form feed: \\f = %d\n", '\f');
    printf("Vtab: \\v = %d\n", '\v');
    printf("Hex A: \\x41 = %c\n", '\x41');

    return 0;
}
