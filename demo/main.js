'use strict';

(function () {
  // ── Example programs (embedded to avoid file:// fetch issues) ──

  var EXAMPLES = {
    hello:
      '// Hello World\n' +
      'int main() {\n' +
      '    printf("Hello, World!\\n");\n' +
      '    return 0;\n' +
      '}\n',

    fibonacci:
      '// Fibonacci sequence\n' +
      'int fib(int n) {\n' +
      '    if (n <= 1) {\n' +
      '        return n;\n' +
      '    }\n' +
      '    return fib(n - 1) + fib(n - 2);\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    int i;\n' +
      '    for (i = 0; i <= 10; i = i + 1) {\n' +
      '        printf("fib(%d) = %d\\n", i, fib(i));\n' +
      '    }\n' +
      '    return 0;\n' +
      '}\n',

    linked_list:
      '// Linked list traversal\n' +
      'struct Node {\n' +
      '    int val;\n' +
      '    int next;\n' +
      '};\n' +
      '\n' +
      'int main() {\n' +
      '    struct Node *a = malloc(sizeof(struct Node));\n' +
      '    struct Node *b = malloc(sizeof(struct Node));\n' +
      '    struct Node *c = malloc(sizeof(struct Node));\n' +
      '    a->val = 10; a->next = b;\n' +
      '    b->val = 20; b->next = c;\n' +
      '    c->val = 30; c->next = 0;\n' +
      '\n' +
      '    int *cur = a;\n' +
      '    int sum = 0;\n' +
      '    while (cur != 0) {\n' +
      '        struct Node *n = cur;\n' +
      '        printf("%d -> ", n->val);\n' +
      '        sum = sum + n->val;\n' +
      '        cur = n->next;\n' +
      '    }\n' +
      '    printf("NULL\\n");\n' +
      '    printf("sum = %d\\n", sum);\n' +
      '    return 0;\n' +
      '}\n',

    sort:
      '// Bubble sort\n' +
      'void swap(int *arr, int i, int j) {\n' +
      '    int tmp = arr[i];\n' +
      '    arr[i] = arr[j];\n' +
      '    arr[j] = tmp;\n' +
      '}\n' +
      '\n' +
      'void bubble_sort(int *arr, int n) {\n' +
      '    int i;\n' +
      '    int j;\n' +
      '    for (i = 0; i < n - 1; i = i + 1) {\n' +
      '        for (j = 0; j < n - 1 - i; j = j + 1) {\n' +
      '            if (arr[j] > arr[j + 1]) {\n' +
      '                swap(arr, j, j + 1);\n' +
      '            }\n' +
      '        }\n' +
      '    }\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    int *arr = malloc(28);\n' +
      '    int i;\n' +
      '    arr[0] = 64;\n' +
      '    arr[1] = 34;\n' +
      '    arr[2] = 25;\n' +
      '    arr[3] = 12;\n' +
      '    arr[4] = 22;\n' +
      '    arr[5] = 11;\n' +
      '    arr[6] = 90;\n' +
      '\n' +
      '    printf("Before: ");\n' +
      '    for (i = 0; i < 7; i = i + 1) {\n' +
      '        printf("%d ", arr[i]);\n' +
      '    }\n' +
      '    printf("\\n");\n' +
      '\n' +
      '    bubble_sort(arr, 7);\n' +
      '\n' +
      '    printf("After:  ");\n' +
      '    for (i = 0; i < 7; i = i + 1) {\n' +
      '        printf("%d ", arr[i]);\n' +
      '    }\n' +
      '    printf("\\n");\n' +
      '    return 0;\n' +
      '}\n',

    collatz:
      '// Collatz sequence (3n+1 problem)\n' +
      '// Features: do-while, ternary (?:), post-increment (count++)\n' +
      '\n' +
      'int steps(int n) {\n' +
      '    int count = 0;\n' +
      '    do {\n' +
      '        n = (n % 2 == 0) ? n / 2 : n * 3 + 1;\n' +
      '        count++;\n' +
      '    } while (n != 1);\n' +
      '    return count;\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    int n;\n' +
      '    int s;\n' +
      '    int max_steps = 0;\n' +
      '    int max_n = 0;\n' +
      '    for (n = 1; n <= 20; n++) {\n' +
      '        s = steps(n);\n' +
      '        printf("collatz(%d) = %d steps\\n", n, s);\n' +
      '        if (s > max_steps) {\n' +
      '            max_steps = s;\n' +
      '            max_n = n;\n' +
      '        }\n' +
      '    }\n' +
      '    printf("Longest: %d with %d steps\\n", max_n, max_steps);\n' +
      '    return 0;\n' +
      '}\n',

    weekday:
      '// Day of Week\n' +
      '// Features: enum, typedef, const, switch/case, ternary\n' +
      '\n' +
      'enum Weekday { MON = 1, TUE, WED, THU, FRI, SAT, SUN };\n' +
      'typedef int Day;\n' +
      '\n' +
      'const int WORK_DAYS = 5;\n' +
      '\n' +
      'int is_weekend(Day d) {\n' +
      '    return (d == SAT || d == SUN) ? 1 : 0;\n' +
      '}\n' +
      '\n' +
      'void print_day(Day d) {\n' +
      '    switch (d) {\n' +
      '        case MON: printf("Monday   "); break;\n' +
      '        case TUE: printf("Tuesday  "); break;\n' +
      '        case WED: printf("Wednesday"); break;\n' +
      '        case THU: printf("Thursday "); break;\n' +
      '        case FRI: printf("Friday   "); break;\n' +
      '        case SAT: printf("Saturday "); break;\n' +
      '        case SUN: printf("Sunday   "); break;\n' +
      '        default:  printf("Unknown  "); break;\n' +
      '    }\n' +
      '    if (is_weekend(d)) {\n' +
      '        printf(" (weekend)\\n");\n' +
      '    } else {\n' +
      '        printf(" (weekday)\\n");\n' +
      '    }\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    Day d;\n' +
      '    int weekends = 0;\n' +
      '    for (d = MON; d <= SUN; d++) {\n' +
      '        weekends += is_weekend(d);\n' +
      '        print_day(d);\n' +
      '    }\n' +
      '    printf("%d work days, %d weekend days\\n", WORK_DAYS, weekends);\n' +
      '    return 0;\n' +
      '}\n',

    bit_flags:
      '// File permissions with bit flags\n' +
      '// Features: typedef, const, compound bitwise ops (|=, &=, ^=), ~, ternary\n' +
      '\n' +
      'typedef int Perms;\n' +
      '\n' +
      'const int READ  = 4;\n' +
      'const int WRITE = 2;\n' +
      'const int EXEC  = 1;\n' +
      '\n' +
      'void show(Perms p) {\n' +
      '    int r;\n' +
      '    int w;\n' +
      '    int x;\n' +
      '    r = (p & READ)  ? 1 : 0;\n' +
      '    w = (p & WRITE) ? 1 : 0;\n' +
      '    x = (p & EXEC)  ? 1 : 0;\n' +
      '    printf("r=%d w=%d x=%d\\n", r, w, x);\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    Perms owner;\n' +
      '    Perms group;\n' +
      '    Perms other;\n' +
      '\n' +
      '    owner = READ | WRITE | EXEC;   /* rwx = 7 */\n' +
      '    group = READ | EXEC;           /* r-x = 5 */\n' +
      '    other = READ;                  /* r-- = 4 */\n' +
      '\n' +
      '    printf("Initial:\\n");\n' +
      '    printf("  owner: "); show(owner);\n' +
      '    printf("  group: "); show(group);\n' +
      '    printf("  other: "); show(other);\n' +
      '\n' +
      '    owner ^= WRITE;       /* revoke write from owner */\n' +
      '    group |= WRITE;       /* grant write to group */\n' +
      '    other &= ~EXEC;       /* ensure no exec for other */\n' +
      '\n' +
      '    printf("After chmod:\\n");\n' +
      '    printf("  owner: "); show(owner);\n' +
      '    printf("  group: "); show(group);\n' +
      '    printf("  other: "); show(other);\n' +
      '    return 0;\n' +
      '}\n',


    floating_point:
      '/* Demonstrates float and double arithmetic */\n' +
      '\n' +
      '/* Approximate pi using Leibniz series: pi/4 = 1 - 1/3 + 1/5 - 1/7 + ... */\n' +
      'double approx_pi(int terms) {\n' +
      '    double sum;\n' +
      '    double sign;\n' +
      '    double denom;\n' +
      '    int i;\n' +
      '    sum = 0.0;\n' +
      '    sign = 1.0;\n' +
      '    denom = 1.0;\n' +
      '    for (i = 0; i < terms; i = i + 1) {\n' +
      '        sum = sum + sign / denom;\n' +
      '        sign = sign * -1.0;\n' +
      '        denom = denom + 2.0;\n' +
      '    }\n' +
      '    return sum * 4.0;\n' +
      '}\n' +
      '\n' +
      '/* Newton\'s method square root approximation */\n' +
      'double my_sqrt(double n) {\n' +
      '    double x;\n' +
      '    double x1;\n' +
      '    int i;\n' +
      '    if (n <= 0.0) return 0.0;\n' +
      '    x = n / 2.0;\n' +
      '    for (i = 0; i < 20; i = i + 1) {\n' +
      '        x1 = (x + n / x) / 2.0;\n' +
      '        if (x1 < 0.0) x1 = x1 * -1.0;\n' +
      '        x = x1;\n' +
      '    }\n' +
      '    return x;\n' +
      '}\n' +
      '\n' +
      '/* Celsius to Fahrenheit */\n' +
      'double c_to_f(double c) {\n' +
      '    return c * 9.0 / 5.0 + 32.0;\n' +
      '}\n' +
      '\n' +
      '/* Fahrenheit to Celsius */\n' +
      'double f_to_c(double f) {\n' +
      '    return (f - 32.0) * 5.0 / 9.0;\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    double pi;\n' +
      '    double sq2;\n' +
      '    double sq3;\n' +
      '    int i;\n' +
      '\n' +
      '    /* Pi approximation */\n' +
      '    printf("Pi approximations (Leibniz series):\\n");\n' +
      '    pi = approx_pi(10);\n' +
      '    printf("  10 terms:    %f\\n", pi);\n' +
      '    pi = approx_pi(100);\n' +
      '    printf("  100 terms:   %f\\n", pi);\n' +
      '    pi = approx_pi(1000);\n' +
      '    printf("  1000 terms:  %f\\n", pi);\n' +
      '\n' +
      '    /* Square roots */\n' +
      '    printf("\\nSquare root (Newton\'s method):\\n");\n' +
      '    sq2 = my_sqrt(2.0);\n' +
      '    printf("  sqrt(2)  = %f\\n", sq2);\n' +
      '    sq3 = my_sqrt(3.0);\n' +
      '    printf("  sqrt(3)  = %f\\n", sq3);\n' +
      '    printf("  sqrt(16) = %f\\n", my_sqrt(16.0));\n' +
      '    printf("  sqrt(0)  = %f\\n", my_sqrt(0.0));\n' +
      '\n' +
      '    /* Temperature conversion */\n' +
      '    printf("\\nTemperature conversions:\\n");\n' +
      '    printf("  0 C  = %f F\\n", c_to_f(0.0));\n' +
      '    printf("  100 C = %f F\\n", c_to_f(100.0));\n' +
      '    printf("  98.6 F = %f C\\n", f_to_c(98.6));\n' +
      '    printf("  -40 C = %f F (same as -40 F)\\n", c_to_f(-40.0));\n' +
      '\n' +
      '    /* Mixed int/float arithmetic */\n' +
      '    printf("\\nMixed arithmetic:\\n");\n' +
      '    printf("  5 / 2 (int) = %d\\n", 5 / 2);\n' +
      '    printf("  5.0 / 2 (float) = %f\\n", 5.0 / 2);\n' +
      '    printf("  (double)5 / 2 = %f\\n", (double)5 / 2);\n' +
      '\n' +
      '    /* Float comparisons */\n' +
      '    printf("\\nFloat comparisons:\\n");\n' +
      '    printf("  0.1 + 0.2 == 0.3: %d\\n", 0.1 + 0.2 == 0.3);\n' +
      '    printf("  1.0 < 2.0: %d\\n", 1.0 < 2.0);\n' +
      '    printf("  -1.5 < 0.0: %d\\n", -1.5 < 0.0);\n' +
      '\n' +
      '    return 0;\n' +
      '}\n',

    integer_types:
      '/* Demonstrates unsigned, short, and long integer types */\n' +
      '\n' +
      'int main() {\n' +
      '    unsigned int u;\n' +
      '    int s;\n' +
      '    unsigned short us;\n' +
      '    short ss;\n' +
      '    long l;\n' +
      '    unsigned int max_u;\n' +
      '    unsigned int bits;\n' +
      '    int i;\n' +
      '\n' +
      '    /* Unsigned vs signed: same bit pattern, different interpretation */\n' +
      '    u = 4294967295; /* 0xFFFFFFFF = max unsigned */\n' +
      '    s = -1;\n' +
      '    printf("unsigned max: %d (as signed: %d)\\n", (int)u, s);\n' +
      '    printf("unsigned > 0: %d\\n", u > 0);       /* 1: unsigned wraps, still > 0 */\n' +
      '    printf("signed > 0: %d\\n", s > 0);         /* 0: signed -1 < 0 */\n' +
      '\n' +
      '    /* Unsigned comparisons */\n' +
      '    printf("\\nUnsigned comparison:\\n");\n' +
      '    max_u = 4294967295;\n' +
      '    printf("  max_uint > 100: %d\\n", max_u > (unsigned int)100);\n' +
      '\n' +
      '    /* Short integer: 16-bit storage */\n' +
      '    printf("\\nShort integers:\\n");\n' +
      '    ss = 32767; /* max short */\n' +
      '    printf("  max short: %d\\n", ss);\n' +
      '    ss = ss + 1; /* overflow */\n' +
      '    printf("  max short + 1: %d\\n", ss); /* wraps to -32768 */\n' +
      '\n' +
      '    us = 65535; /* max unsigned short */\n' +
      '    printf("  max unsigned short: %d\\n", (int)us);\n' +
      '    us = us + 1; /* wraps to 0 */\n' +
      '    printf("  max unsigned short + 1: %d\\n", (int)us);\n' +
      '\n' +
      '    /* Bit manipulation with unsigned */\n' +
      '    printf("\\nBit manipulation:\\n");\n' +
      '    bits = 0;\n' +
      '    for (i = 0; i < 8; i = i + 1) {\n' +
      '        bits = bits | (1 << i);\n' +
      '    }\n' +
      '    printf("  bits set 0-7: %d (0x%x)\\n", (int)bits, (int)bits);\n' +
      '    bits = bits >> 1;\n' +
      '    printf("  logical right shift: %d (0x%x)\\n", (int)bits, (int)bits);\n' +
      '\n' +
      '    /* Long (same as int in c2wasm, but valid syntax) */\n' +
      '    printf("\\nLong integers:\\n");\n' +
      '    l = 2147483647; /* max int = max long in c2wasm */\n' +
      '    printf("  max long: %d\\n", (int)l);\n' +
      '\n' +
      '    return 0;\n' +
      '}\n',

    memory_pools:
      '/* Demonstrates real free() with coalescing free-list allocator */\n' +
      '\n' +
      'struct Node {\n' +
      '    int value;\n' +
      '    struct Node *next;\n' +
      '};\n' +
      '\n' +
      '/* Build a linked list of n nodes */\n' +
      'struct Node *make_list(int n) {\n' +
      '    struct Node *head;\n' +
      '    struct Node *node;\n' +
      '    int i;\n' +
      '    head = (struct Node *)0;\n' +
      '    for (i = 0; i < n; i = i + 1) {\n' +
      '        node = (struct Node *)malloc(sizeof(struct Node));\n' +
      '        node->value = i;\n' +
      '        node->next = head;\n' +
      '        head = node;\n' +
      '    }\n' +
      '    return head;\n' +
      '}\n' +
      '\n' +
      '/* Free all nodes in a list */\n' +
      'void free_list(struct Node *head) {\n' +
      '    struct Node *next;\n' +
      '    while (head != (struct Node *)0) {\n' +
      '        next = head->next;\n' +
      '        free(head);\n' +
      '        head = next;\n' +
      '    }\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    struct Node *list1;\n' +
      '    struct Node *list2;\n' +
      '    struct Node *p;\n' +
      '    int *a;\n' +
      '    int *b;\n' +
      '\n' +
      '    /* Allocate and free - memory should be reused */\n' +
      '    a = (int *)malloc(64);\n' +
      '    *a = 12345;\n' +
      '    printf("First allocation at: %d\\n", (int)a);\n' +
      '    free(a);\n' +
      '\n' +
      '    b = (int *)malloc(64);\n' +
      '    printf("After free, reused at: %d\\n", (int)b);\n' +
      '    if ((int)a == (int)b) {\n' +
      '        printf("Memory was reused!\\n");\n' +
      '    }\n' +
      '    free(b);\n' +
      '\n' +
      '    /* Build and free a linked list */\n' +
      '    printf("\\nBuilding list of 5 nodes...\\n");\n' +
      '    list1 = make_list(5);\n' +
      '    p = list1;\n' +
      '    while (p != (struct Node *)0) {\n' +
      '        printf("  node value: %d\\n", p->value);\n' +
      '        p = p->next;\n' +
      '    }\n' +
      '    free_list(list1);\n' +
      '    printf("List freed.\\n");\n' +
      '\n' +
      '    /* Build another list - should reuse memory */\n' +
      '    printf("\\nBuilding second list of 5 nodes...\\n");\n' +
      '    list2 = make_list(5);\n' +
      '    printf("Second list built (memory reused from first).\\n");\n' +
      '    free_list(list2);\n' +
      '\n' +
      '    printf("Done!\\n");\n' +
      '    return 0;\n' +
      '}\n',

    string_ops:
      '/* Demonstrates libc string and ctype functions */\n' +
      '\n' +
      '/* Simple helper: count words in a string */\n' +
      'int count_words(char *s) {\n' +
      '    int count;\n' +
      '    int in_word;\n' +
      '    count = 0;\n' +
      '    in_word = 0;\n' +
      '    while (*s != \'\\0\') {\n' +
      '        if (isspace(*s)) {\n' +
      '            in_word = 0;\n' +
      '        } else if (!in_word) {\n' +
      '            in_word = 1;\n' +
      '            count = count + 1;\n' +
      '        }\n' +
      '        s = s + 1;\n' +
      '    }\n' +
      '    return count;\n' +
      '}\n' +
      '\n' +
      '/* Convert string to uppercase in place */\n' +
      'void str_upper(char *s) {\n' +
      '    while (*s != \'\\0\') {\n' +
      '        *s = toupper(*s);\n' +
      '        s = s + 1;\n' +
      '    }\n' +
      '}\n' +
      '\n' +
      '/* Check if string is all digits */\n' +
      'int is_number(char *s) {\n' +
      '    if (*s == \'\\0\') return 0;\n' +
      '    while (*s != \'\\0\') {\n' +
      '        if (!isdigit(*s)) return 0;\n' +
      '        s = s + 1;\n' +
      '    }\n' +
      '    return 1;\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    char buf[64];\n' +
      '    char *p;\n' +
      '    int n;\n' +
      '\n' +
      '    /* strlen */\n' +
      '    printf("strlen(\\"hello\\") = %d\\n", strlen("hello"));\n' +
      '\n' +
      '    /* strcpy + strcat */\n' +
      '    strcpy(buf, "Hello");\n' +
      '    strcat(buf, ", ");\n' +
      '    strcat(buf, "world!");\n' +
      '    printf("strcpy+strcat: %s\\n", buf);\n' +
      '\n' +
      '    /* strcmp */\n' +
      '    printf("\\nComparing strings:\\n");\n' +
      '    printf("  strcmp(\\"abc\\", \\"abc\\") = %d\\n", strcmp("abc", "abc"));\n' +
      '    printf("  strcmp(\\"abc\\", \\"abd\\") = %d\\n", strcmp("abc", "abd"));\n' +
      '    printf("  strcmp(\\"abd\\", \\"abc\\") = %d\\n", strcmp("abd", "abc"));\n' +
      '\n' +
      '    /* strchr and strstr */\n' +
      '    p = strchr("hello world", \'o\');\n' +
      '    if (p != (char *)0) {\n' +
      '        printf("\\nstrchr found \'o\'\\n");\n' +
      '    }\n' +
      '    p = strstr("hello world", "world");\n' +
      '    if (p != (char *)0) {\n' +
      '        printf("strstr found \\"world\\"\\n");\n' +
      '    }\n' +
      '\n' +
      '    /* ctype functions */\n' +
      '    printf("\\nctype functions:\\n");\n' +
      '    printf("  isdigit(\'5\'): %d\\n", isdigit(\'5\'));\n' +
      '    printf("  isalpha(\'A\'): %d\\n", isalpha(\'A\'));\n' +
      '    printf("  isspace(\' \'): %d\\n", isspace(\' \'));\n' +
      '    printf("  toupper(\'a\'): %c\\n", toupper(\'a\'));\n' +
      '    printf("  tolower(\'Z\'): %c\\n", tolower(\'Z\'));\n' +
      '\n' +
      '    /* Custom functions using libc */\n' +
      '    printf("\\nWord count:\\n");\n' +
      '    printf("  \\"hello world\\": %d words\\n", count_words("hello world"));\n' +
      '    printf("  \\"  spaces  \\": %d words\\n", count_words("  spaces  "));\n' +
      '    printf("  \\"one two three four\\": %d words\\n", count_words("one two three four"));\n' +
      '\n' +
      '    /* is_number */\n' +
      '    printf("\\nNumber detection:\\n");\n' +
      '    printf("  \\"12345\\" is number: %d\\n", is_number("12345"));\n' +
      '    printf("  \\"12x45\\" is number: %d\\n", is_number("12x45"));\n' +
      '\n' +
      '    /* atoi */\n' +
      '    printf("\\natoi:\\n");\n' +
      '    n = atoi("42");\n' +
      '    printf("  atoi(\\"42\\") = %d\\n", n);\n' +
      '    n = atoi("-100");\n' +
      '    printf("  atoi(\\"-100\\") = %d\\n", n);\n' +
      '\n' +
      '    /* str_upper */\n' +
      '    strcpy(buf, "hello world");\n' +
      '    str_upper(buf);\n' +
      '    printf("\\nUPPERCASE: %s\\n", buf);\n' +
      '\n' +
      '    return 0;\n' +
      '}\n',

    greet:
      '// Reads a name from stdin and prints a greeting.\n' +
      '// Type your name in the terminal below, then press Enter!\n' +
      'int main() {\n' +
      '    char name[64];\n' +
      '    int i;\n' +
      '    int c;\n' +
      '    printf("What is your name? ");\n' +
      '    i = 0;\n' +
      '    while (i < 63) {\n' +
      '        c = getchar();\n' +
      '        if (c == -1 || c == 10) {\n' +
      '            break;\n' +
      '        }\n' +
      '        name[i] = c;\n' +
      '        i = i + 1;\n' +
      '    }\n' +
      '    name[i] = 0;\n' +
      '    printf("Hello, %s!\\n", name);\n' +
      '    return 0;\n' +
      '}\n'
  };

  var STORAGE_KEYS = {
    FILES: 'c2wasm:files',
    ACTIVE: 'c2wasm:active',
    COMPILER_SOURCE: 'c2wasm:compiler-source'
  };

  function loadUserFiles() {
    try {
      var raw = localStorage.getItem(STORAGE_KEYS.FILES);
      return raw ? JSON.parse(raw) : [];
    } catch (e) { return []; }
  }

  function saveUserFiles(files) {
    try {
      localStorage.setItem(STORAGE_KEYS.FILES, JSON.stringify(files));
      return true;
    } catch (e) {
      console.warn('c2wasm: localStorage write failed', e);
      return false;
    }
  }

  function getActiveFile() {
    try {
      var raw = localStorage.getItem(STORAGE_KEYS.ACTIVE);
      return raw ? JSON.parse(raw) : { type: 'example', id: 'hello' };
    } catch (e) { return { type: 'example', id: 'hello' }; }
  }

  // ── Compiler source helpers ──

  var compilerSourceCache = null; // original fetched source
  var compilerSourceLoading = null;

  function fetchCompilerSource() {
    if (compilerSourceCache) return Promise.resolve(compilerSourceCache);
    if (compilerSourceLoading) return compilerSourceLoading;

    compilerSourceLoading = fetch('./c2wasm.c')
      .then(function (r) {
        if (!r.ok) throw new Error('Could not load c2wasm.c (HTTP ' + r.status + ')');
        return r.text();
      })
      .then(function (text) {
        compilerSourceCache = text;
        return text;
      });

    compilerSourceLoading.catch(function () { compilerSourceLoading = null; });
    return compilerSourceLoading;
  }

  function loadCompilerSource() {
    try {
      return localStorage.getItem(STORAGE_KEYS.COMPILER_SOURCE);
    } catch (e) { return null; }
  }

  function saveCompilerSource(source) {
    try {
      localStorage.setItem(STORAGE_KEYS.COMPILER_SOURCE, source);
      return true;
    } catch (e) {
      console.warn('c2wasm: compiler source save failed', e);
      return false;
    }
  }

  function getCompilerEditorContent() {
    var saved = loadCompilerSource();
    if (saved !== null) return Promise.resolve(saved);
    return fetchCompilerSource();
  }

  function setActiveFile(type, id) {
    try {
      localStorage.setItem(STORAGE_KEYS.ACTIVE, JSON.stringify({ type: type, id: id }));
    } catch (e) {}
  }

  function generateId() {
    return Date.now().toString(36) + Math.random().toString(36).substr(2, 6);
  }

  function getFileContent(type, id) {
    if (type === 'example') {
      return EXAMPLES[id] || EXAMPLES.hello;
    }
    var files = loadUserFiles();
    for (var i = 0; i < files.length; i++) {
      if (files[i].id === id) return files[i].content;
    }
    return null; // null = file not found; distinguish from empty string (valid empty file)
  }

  // ── DOM references ──

  var editorContainer = document.getElementById('editor-container');
  var fileSelect = document.getElementById('file-select');
  var userFilesGroup = document.getElementById('user-files-group');
  var newFileBtn = document.getElementById('new-file-btn');
  var deleteFileBtn = document.getElementById('delete-file-btn');
  var runBtn = document.getElementById('run-btn');
  var buildCompilerBtn = document.getElementById('build-compiler-btn');
  var compilerModeEl = document.getElementById('compiler-mode');
  var compilerModeLabel = document.getElementById('compiler-mode-label');
  var revertCompilerBtn = document.getElementById('revert-compiler-btn');
  var statusEl = document.getElementById('status');
  var saveIndicator = document.getElementById('save-indicator');
  var consoleTabContent = document.getElementById('console-tab-content');
  var consoleOutput = document.getElementById('console-output');
  var terminalInputRow = document.getElementById('terminal-input-row');
  var terminalInput = document.getElementById('terminal-input');
  var resizeHandle = document.getElementById('resize-handle');

  var editor = null;
  var isRunning = false;
  var idleTimerId = null;
  var autoSaveTimer = null;

  // ── File selector helpers ──

  function refreshFileSelector() {
    var files = loadUserFiles();
    userFilesGroup.innerHTML = '';
    for (var i = 0; i < files.length; i++) {
      var opt = document.createElement('option');
      opt.value = 'user:' + files[i].id;
      opt.textContent = files[i].name;
      userFilesGroup.appendChild(opt);
    }
    // optgroup is always in DOM; toggling disabled hides it in most browsers
    userFilesGroup.disabled = files.length === 0;
  }

  function syncSelectToActive() {
    var active = getActiveFile();
    fileSelect.value = active.type === 'user' ? 'user:' + active.id
      : active.type === 'compiler' ? 'compiler:' + active.id
      : active.id;
    deleteFileBtn.style.display = active.type === 'user' ? '' : 'none';
    updateCompilerUI();
  }

  function isCompilerFile() {
    var active = getActiveFile();
    return active.type === 'compiler';
  }

  function updateCompilerUI() {
    var onCompiler = isCompilerFile();
    // Toggle button visibility
    if (onCompiler) {
      runBtn.classList.add('hidden');
      buildCompilerBtn.classList.remove('hidden');
    } else {
      runBtn.classList.remove('hidden');
      buildCompilerBtn.classList.add('hidden');
    }
    // Compiler mode indicator
    var mode = CompilerAPI.getMode();
    if (mode === 'custom') {
      compilerModeEl.classList.remove('hidden');
      compilerModeLabel.textContent = 'Custom ✨';
      compilerModeLabel.className = 'compiler-mode-label custom';
      revertCompilerBtn.classList.remove('hidden');
    } else if (CompilerAPI.hasCustomCompiler()) {
      compilerModeEl.classList.remove('hidden');
      compilerModeLabel.textContent = 'Reference';
      compilerModeLabel.className = 'compiler-mode-label';
      revertCompilerBtn.classList.remove('hidden');
    } else {
      compilerModeEl.classList.add('hidden');
      revertCompilerBtn.classList.add('hidden');
    }
  }

  function showSaveIndicator() {
    saveIndicator.textContent = '● Saved';
    saveIndicator.className = 'save-indicator saved';
    setTimeout(function () {
      saveIndicator.textContent = '';
      saveIndicator.className = 'save-indicator';
    }, 1500);
  }

  function showSaveError() {
    saveIndicator.textContent = '⚠ Save failed';
    saveIndicator.className = 'save-indicator save-error';
    setTimeout(function () {
      saveIndicator.textContent = '';
      saveIndicator.className = 'save-indicator';
    }, 3000);
  }

  // Flush any pending debounced auto-save synchronously before switching files.
  // Guards against losing edits when the user switches files within 800ms of typing.
  function flushPendingSave() {
    if (!autoSaveTimer) return;
    clearTimeout(autoSaveTimer);
    autoSaveTimer = null;
    var active = getActiveFile();
    if (active.type === 'compiler' && editor) {
      saveCompilerSource(editor.getValue());
      return;
    }
    if (active.type !== 'user' || !editor) return;
    var files = loadUserFiles();
    for (var i = 0; i < files.length; i++) {
      if (files[i].id === active.id) {
        files[i].content = editor.getValue();
        files[i].modified = Date.now();
        saveUserFiles(files); // best-effort; errors already warned to console
        break;
      }
    }
  }

  function scheduleAutoSave() {
    if (autoSaveTimer) clearTimeout(autoSaveTimer);
    autoSaveTimer = setTimeout(function () {
      autoSaveTimer = null;
      var active = getActiveFile();
      if (active.type === 'compiler' && editor) {
        if (saveCompilerSource(editor.getValue())) {
          showSaveIndicator();
        } else {
          showSaveError();
        }
        return;
      }
      if (active.type !== 'user' || !editor) return;
      var files = loadUserFiles();
      for (var i = 0; i < files.length; i++) {
        if (files[i].id === active.id) {
          files[i].content = editor.getValue();
          files[i].modified = Date.now();
          if (saveUserFiles(files)) {
            showSaveIndicator();
          } else {
            showSaveError();
          }
          break;
        }
      }
    }, 800);
  }

  // ── Tab switching ──

  // ── File selector events ──

  fileSelect.addEventListener('change', function () {
    flushPendingSave();
    var val = fileSelect.value;
    var type, id;
    if (val.indexOf('compiler:') === 0) {
      type = 'compiler';
      id = val.slice(9);
    } else if (val.indexOf('user:') === 0) {
      type = 'user';
      id = val.slice(5);
    } else {
      type = 'example';
      id = val;
    }
    setActiveFile(type, id);
    deleteFileBtn.style.display = type === 'user' ? '' : 'none';

    if (type === 'compiler') {
      // Load compiler source asynchronously
      if (editor) {
        editor.setValue('// Loading compiler source…');
        editor.updateOptions({ readOnly: true });
      }
      getCompilerEditorContent().then(function (source) {
        if (editor && isCompilerFile()) {
          editor.setValue(source);
          editor.updateOptions({ readOnly: false });
          monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
        }
      }).catch(function (err) {
        if (editor && isCompilerFile()) {
          editor.setValue('// Error loading compiler source: ' + (err.message || err));
          editor.updateOptions({ readOnly: false });
        }
      });
      updateCompilerUI();
      return;
    }

    var code = getFileContent(type, id);
    if (code === null) {
      // User file was deleted from another tab or storage; fall back gracefully
      setActiveFile('example', 'hello');
      fileSelect.value = 'hello';
      deleteFileBtn.style.display = 'none';
      code = EXAMPLES.hello;
    }
    if (editor) {
      editor.setValue(code);
      monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
    }
    updateCompilerUI();
  });

  newFileBtn.addEventListener('click', function () {
    flushPendingSave();
    var name = prompt('File name:', 'program.c');
    if (!name) return;
    name = name.trim();
    if (!name) return;
    if (name.indexOf('.') === -1) name += '.c';

    var id = generateId();
    var files = loadUserFiles();
    var newFile = {
      id: id,
      name: name,
      content: '// ' + name + '\n\nint main() {\n    return 0;\n}\n',
      created: Date.now(),
      modified: Date.now()
    };
    files.push(newFile);
    saveUserFiles(files);

    refreshFileSelector();
    fileSelect.value = 'user:' + id;
    setActiveFile('user', id);
    deleteFileBtn.style.display = '';

    if (editor) {
      editor.setValue(newFile.content);
      monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
      editor.focus();
    }
  });

  deleteFileBtn.addEventListener('click', function () {
    flushPendingSave();
    var active = getActiveFile();
    if (active.type !== 'user') return;
    var files = loadUserFiles();
    var file = null;
    for (var i = 0; i < files.length; i++) {
      if (files[i].id === active.id) { file = files[i]; break; }
    }
    if (!confirm('Delete "' + (file ? file.name : 'this file') + '"? This cannot be undone.')) return;
    saveUserFiles(files.filter(function (f) { return f.id !== active.id; }));

    refreshFileSelector();
    fileSelect.value = 'hello';
    setActiveFile('example', 'hello');
    deleteFileBtn.style.display = 'none';

    if (editor) {
      editor.setValue(EXAMPLES.hello);
      monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
    }
    updateCompilerUI();
  });

  // ── Build Compiler / Revert ──

  var buildIdleTimerId = null;

  function setBuildButtonState(state) {
    if (buildIdleTimerId) {
      clearTimeout(buildIdleTimerId);
      buildIdleTimerId = null;
    }
    buildCompilerBtn.classList.remove('success', 'error');
    switch (state) {
      case 'building':
        buildCompilerBtn.textContent = '⏳ Building…';
        buildCompilerBtn.disabled = true;
        break;
      case 'success':
        buildCompilerBtn.textContent = '✓ Built';
        buildCompilerBtn.disabled = false;
        buildCompilerBtn.classList.add('success');
        buildIdleTimerId = setTimeout(function () { setBuildButtonState('idle'); }, 2000);
        break;
      case 'error':
        buildCompilerBtn.textContent = '✗ Failed';
        buildCompilerBtn.disabled = false;
        buildCompilerBtn.classList.add('error');
        buildIdleTimerId = setTimeout(function () { setBuildButtonState('idle'); }, 2000);
        break;
      default:
        buildCompilerBtn.textContent = '🔨 Build Compiler';
        buildCompilerBtn.disabled = false;
    }
  }

  buildCompilerBtn.addEventListener('click', function () {
    if (!editor) return;
    var source = editor.getValue();
    clearOutput();
    if (editor.getModel()) {
      monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
    }

    setBuildButtonState('building');
    setStatus('Building custom compiler…');

    CompilerAPI.buildCompiler(source)
      .then(function () {
        writeConsole('Custom compiler built successfully!\n', 'output-success');
        writeConsole('Smoke test: compiling Hello World…\n', 'output-info');
        return CompilerAPI.compileBinary('int main() { printf("Hello from custom compiler!\\n"); return 0; }');
      })
      .then(function (testBytes) {
        if (!WebAssembly.validate(testBytes)) {
          throw new Error('Smoke test failed: custom compiler produced invalid WASM binary');
        }
        writeConsole('Smoke test passed — ' + testBytes.length + ' bytes of valid WASM.\n', 'output-success');
        writeConsole('\nYour custom compiler is now active. Switch to a program file to try it out.\n', 'output-info');
        setBuildButtonState('success');
        setStatus('Custom compiler active');
        updateCompilerUI();
      })
      .catch(function (err) {
        var msg = err.message || String(err);
        var errors = parseErrors(msg);
        if (errors.length > 0) {
          setErrorMarkers(errors);
        }
        writeConsole('Build failed:\n' + msg, 'output-error');
        // Ensure we're still on reference compiler
        CompilerAPI.useReference();
        setBuildButtonState('error');
        setStatus('Build failed');
        updateCompilerUI();
      });
  });

  revertCompilerBtn.addEventListener('click', function () {
    CompilerAPI.useReference();
    updateCompilerUI();
    setStatus('Reverted to reference compiler');
    clearOutput();
    writeConsole('Reverted to reference compiler.\n', 'output-info');
  });

  // ── Panel resize ──

  (function () {
    var dragging = false;
    var editorPanel = document.querySelector('.editor-panel');

    resizeHandle.addEventListener('mousedown', function (e) {
      e.preventDefault();
      dragging = true;
      resizeHandle.classList.add('active');
      document.body.style.cursor = 'col-resize';
      document.body.style.userSelect = 'none';
    });

    document.addEventListener('mousemove', function (e) {
      if (!dragging) return;
      var mainRect = editorPanel.parentElement.getBoundingClientRect();
      var pct = ((e.clientX - mainRect.left) / mainRect.width) * 100;
      pct = Math.max(20, Math.min(80, pct));
      editorPanel.style.flex = '0 0 ' + pct + '%';
      if (editor) editor.layout();
    });

    document.addEventListener('mouseup', function () {
      if (!dragging) return;
      dragging = false;
      resizeHandle.classList.remove('active');
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    });
  })();

  // ── Monaco editor ──

  function initMonaco() {
    require.config({
      paths: { vs: 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs' }
    });

    require(['vs/editor/editor.main'], function () {
      // Restore last active file from localStorage
      var active = getActiveFile();
      var initialContent;
      var loadCompilerAsync = false;
      if (active.type === 'compiler') {
        // Will load async after editor is created
        initialContent = '// Loading compiler source…';
        loadCompilerAsync = true;
      } else if (active.type === 'user') {
        initialContent = getFileContent('user', active.id);
        if (initialContent === null) {
          // File was deleted; fall back to hello example
          active = { type: 'example', id: 'hello' };
          setActiveFile('example', 'hello');
          initialContent = EXAMPLES.hello;
        }
      } else {
        initialContent = EXAMPLES[active.id] || EXAMPLES.hello;
      }

      editor = monaco.editor.create(editorContainer, {
        value: initialContent,
        language: 'c',
        theme: 'vs-dark',
        fontSize: 14,
        fontFamily: "'JetBrains Mono', 'Cascadia Code', monospace",
        minimap: { enabled: false },
        automaticLayout: true,
        scrollBeyondLastLine: false,
        lineNumbers: 'on',
        renderWhitespace: 'none',
        tabSize: 4,
        padding: { top: 12, bottom: 12 }
      });

      syncSelectToActive();

      // Load compiler source async if that was the last active file
      if (loadCompilerAsync) {
        editor.updateOptions({ readOnly: true });
        getCompilerEditorContent().then(function (source) {
          if (editor && isCompilerFile()) {
            editor.setValue(source);
            editor.updateOptions({ readOnly: false });
          }
        }).catch(function (err) {
          if (editor && isCompilerFile()) {
            editor.setValue('// Error loading compiler source: ' + (err.message || err));
            editor.updateOptions({ readOnly: false });
          }
        });
      }

      editor.onDidChangeModelContent(function () {
        scheduleAutoSave();
      });

      editor.addAction({
        id: 'compile-run',
        label: 'Compile & Run',
        keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter],
        run: function () {
          if (isCompilerFile()) {
            buildCompilerBtn.click();
          } else {
            compileAndRun();
          }
        }
      });

      setStatus('Ready — Ctrl+Enter to compile');
    });
  }

  // ── wabt.js (lazy-loaded) ──

  // ── Interactive WASM runner (Web Worker + SharedArrayBuffer) ──

  // SAB layout: Int32[0]=signal, Int32[1]=byte count, bytes from offset 8.
  var SAB_SIZE = 8 + 4096;

  var activeWorker = null;

  function showTerminalInput(sab, sabInt32) {
    terminalInputRow.classList.remove('hidden');
    terminalInput.value = '';
    terminalInput.focus();
    // Scroll so the input line is visible
    consoleOutput.scrollTop = consoleOutput.scrollHeight;

    function onEnter(e) {
      if (e.key !== 'Enter') return;
      terminalInput.removeEventListener('keydown', onEnter);

      var text = terminalInput.value + '\n';
      hideTerminalInput();

      // Echo what the user typed into the terminal output
      writeConsole(text);

      // Write input to SAB and wake the worker
      var encoded = new TextEncoder().encode(text);
      var toWrite = Math.min(encoded.length, SAB_SIZE - 8);
      new Uint8Array(sab, 8, toWrite).set(encoded.subarray(0, toWrite));
      Atomics.store(sabInt32, 1, toWrite);
      Atomics.store(sabInt32, 0, 1);
      Atomics.notify(sabInt32, 0, 1);
    }

    terminalInput.addEventListener('keydown', onEnter);
  }

  function hideTerminalInput() {
    terminalInputRow.classList.add('hidden');
    terminalInput.value = '';
  }

  function runWasmInteractive(wasmBytes) {
    if (typeof SharedArrayBuffer === 'undefined') {
      return Promise.reject(new Error(
        'SharedArrayBuffer is not available in this context.\n' +
        'Run locally with: cd compiler && make serve\n' +
        '(The dev server sets the required cross-origin isolation headers.)'
      ));
    }

    return new Promise(function (resolve, reject) {
      var sab = new SharedArrayBuffer(SAB_SIZE);
      var sabInt32 = new Int32Array(sab);
      var hasOutput = false;

      var worker = new Worker('./wasm-worker.js');
      activeWorker = worker;

      worker.onmessage = function (event) {
        var msg = event.data;
        if (msg.type === 'output') {
          hasOutput = true;
          writeConsole(msg.text);
        } else if (msg.type === 'needInput') {
          showTerminalInput(sab, sabInt32);
        } else if (msg.type === 'exit') {
          activeWorker = null;
          worker.terminate();
          hideTerminalInput();
          if (!hasOutput) writeConsole('(no output)', 'output-info');
          if (msg.code !== 0) {
            writeConsole('\n[Process exited with code ' + msg.code + ']', 'output-info');
          }
          resolve(msg.code);
        } else if (msg.type === 'error') {
          activeWorker = null;
          worker.terminate();
          hideTerminalInput();
          reject(new Error(msg.message));
        }
      };

      worker.onerror = function (e) {
        activeWorker = null;
        worker.terminate();
        hideTerminalInput();
        reject(new Error(e.message || 'Worker error'));
      };

      // Transfer wasmBytes to the worker (zero-copy)
      worker.postMessage({ type: 'run', wasmBytes: wasmBytes, sab: sab }, [wasmBytes]);
    });
  }

  // ── Error parsing + Monaco markers ──

  function parseErrors(text) {
    var errors = [];
    var lines = text.split('\n');
    for (var i = 0; i < lines.length; i++) {
      var m = lines[i].match(/line\s+(\d+)(?:[,:]\s*col(?:umn)?\s+(\d+))?[:\s]+(.*)/i);
      if (m) {
        errors.push({
          line: parseInt(m[1], 10),
          col: m[2] ? parseInt(m[2], 10) : 1,
          message: m[3].trim()
        });
      }
    }
    return errors;
  }

  function setErrorMarkers(errors) {
    if (!editor) return;
    var markers = errors.map(function (err) {
      return {
        startLineNumber: err.line,
        startColumn: err.col,
        endLineNumber: err.line,
        endColumn: err.col + 100,
        message: err.message,
        severity: monaco.MarkerSeverity.Error
      };
    });
    monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', markers);
  }

  // ── UI helpers ──

  function setStatus(text) {
    statusEl.textContent = text;
  }

  function setButtonState(state) {
    if (idleTimerId) {
      clearTimeout(idleTimerId);
      idleTimerId = null;
    }
    runBtn.className = '';
    switch (state) {
      case 'compiling':
        runBtn.textContent = '⏳ Compiling…';
        runBtn.disabled = true;
        break;
      case 'running':
        runBtn.textContent = '⏳ Running…';
        runBtn.disabled = true;
        break;
      case 'success':
        runBtn.textContent = '✓ Done';
        runBtn.disabled = false;
        runBtn.className = 'success';
        idleTimerId = setTimeout(function () { setButtonState('idle'); }, 2000);
        break;
      case 'error':
        runBtn.textContent = '✗ Error';
        runBtn.disabled = false;
        runBtn.className = 'error';
        idleTimerId = setTimeout(function () { setButtonState('idle'); }, 2000);
        break;
      default:
        runBtn.textContent = '▶ Compile & Run';
        runBtn.disabled = false;
    }
  }

  function writeConsole(text, className) {
    var span = document.createElement('span');
    if (className) span.className = className;
    span.textContent = text;
    consoleOutput.appendChild(span);
    // Auto-scroll if already near the bottom
    var nearBottom = consoleOutput.scrollHeight - consoleOutput.scrollTop - consoleOutput.clientHeight < 60;
    if (nearBottom) consoleOutput.scrollTop = consoleOutput.scrollHeight;
  }

  function clearOutput() {
    consoleOutput.textContent = '';
  }

  // ── Compile & Run pipeline ──

  function compileAndRun() {
    if (!editor || isRunning) return;
    isRunning = true;

    var source = editor.getValue();
    clearOutput();
    monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);

    setButtonState('compiling');
    setStatus('Compiling…');

    CompilerAPI.compileBinary(source)
      .then(function (bytes) {
        return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
      })
      .then(function (wasmBytes) {
        setButtonState('running');
        setStatus('Running…');

        return runWasmInteractive(wasmBytes).then(function (exitCode) {
          setButtonState(exitCode === 0 ? 'success' : 'error');
          setStatus(exitCode === 0 ? 'Done' : 'Exited with code ' + exitCode);
        });
      })
      .catch(function (err) {
        var msg = err.message || String(err);

        // Try to parse compiler errors for Monaco markers
        var errors = parseErrors(msg);
        if (errors.length > 0) {
          setErrorMarkers(errors);
        }

        writeConsole(msg, 'output-error');
        setButtonState('error');
        setStatus('Error');
      })
      .then(function () { isRunning = false; }, function () { isRunning = false; });
  }

  // ── Bind events ──

  runBtn.addEventListener('click', compileAndRun);

  // ── Service worker (cross-origin isolation for SharedArrayBuffer) ──

  if (!self.crossOriginIsolated && 'serviceWorker' in navigator) {
    // Register the COI service worker and reload once so SharedArrayBuffer
    // becomes available (needed for interactive stdin via Atomics.wait).
    var reloadKey = 'coiReloaded';
    if (!sessionStorage.getItem(reloadKey)) {
      sessionStorage.setItem(reloadKey, '1');
      navigator.serviceWorker.register('./coi-serviceworker.js').then(function () {
        window.location.reload();
      });
    }
  }

  // ── Boot ──

  setStatus('Loading editor…');
  refreshFileSelector();
  initMonaco();
})();
