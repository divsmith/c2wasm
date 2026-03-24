/* Demonstrates libc string and ctype functions */

/* Simple helper: count words in a string */
int count_words(char *s) {
    int count;
    int in_word;
    count = 0;
    in_word = 0;
    while (*s != '\0') {
        if (isspace(*s)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            count = count + 1;
        }
        s = s + 1;
    }
    return count;
}

/* Convert string to uppercase in place */
void str_upper(char *s) {
    while (*s != '\0') {
        *s = toupper(*s);
        s = s + 1;
    }
}

/* Check if string is all digits */
int is_number(char *s) {
    if (*s == '\0') return 0;
    while (*s != '\0') {
        if (!isdigit(*s)) return 0;
        s = s + 1;
    }
    return 1;
}

int main() {
    char buf[64];
    char *p;
    int n;

    /* strlen */
    printf("strlen(\"hello\") = %d\n", strlen("hello"));

    /* strcpy + strcat */
    strcpy(buf, "Hello");
    strcat(buf, ", ");
    strcat(buf, "world!");
    printf("strcpy+strcat: %s\n", buf);

    /* strcmp */
    printf("\nComparing strings:\n");
    printf("  strcmp(\"abc\", \"abc\") = %d\n", strcmp("abc", "abc"));
    printf("  strcmp(\"abc\", \"abd\") = %d\n", strcmp("abc", "abd"));
    printf("  strcmp(\"abd\", \"abc\") = %d\n", strcmp("abd", "abc"));

    /* strchr and strstr */
    p = strchr("hello world", 'o');
    if (p != (char *)0) {
        printf("\nstrchr found 'o'\n");
    }
    p = strstr("hello world", "world");
    if (p != (char *)0) {
        printf("strstr found \"world\"\n");
    }

    /* ctype functions */
    printf("\nctype functions:\n");
    printf("  isdigit('5'): %d\n", isdigit('5'));
    printf("  isalpha('A'): %d\n", isalpha('A'));
    printf("  isspace(' '): %d\n", isspace(' '));
    printf("  toupper('a'): %c\n", toupper('a'));
    printf("  tolower('Z'): %c\n", tolower('Z'));

    /* Custom functions using libc */
    printf("\nWord count:\n");
    printf("  \"hello world\": %d words\n", count_words("hello world"));
    printf("  \"  spaces  \": %d words\n", count_words("  spaces  "));
    printf("  \"one two three four\": %d words\n", count_words("one two three four"));

    /* is_number */
    printf("\nNumber detection:\n");
    printf("  \"12345\" is number: %d\n", is_number("12345"));
    printf("  \"12x45\" is number: %d\n", is_number("12x45"));

    /* atoi */
    printf("\natoi:\n");
    n = atoi("42");
    printf("  atoi(\"42\") = %d\n", n);
    n = atoi("-100");
    printf("  atoi(\"-100\") = %d\n", n);

    /* str_upper */
    strcpy(buf, "hello world");
    str_upper(buf);
    printf("\nUPPERCASE: %s\n", buf);

    return 0;
}
