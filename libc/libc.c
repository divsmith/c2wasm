/*
 * libc.c — Minimal freestanding C standard library for c2wasm
 *
 * Compiled to libc.wasm with wasi-sdk and linked with user programs
 * in the browser demo. No system headers — fully self-contained.
 *
 * malloc/free are provided by the host runtime, declared as extern.
 */

typedef unsigned int size_t;
#define NULL ((void *)0)

/* Functions provided by the host runtime */
extern void *malloc(size_t size);
extern void free(void *ptr);
extern void putchar(int c);

/* ------------------------------------------------------------------ */
/*  string.h                                                          */
/* ------------------------------------------------------------------ */

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len = len + 1;
    }
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 != '\0' && *s1 == *s2) {
        s1 = s1 + 1;
        s2 = s2 + 1;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 != '\0' && *s1 == *s2) {
        s1 = s1 + 1;
        s2 = s2 + 1;
        n = n - 1;
    }
    if (n == 0) return 0;
    return (unsigned char)*s1 - (unsigned char)*s2;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while (*src != '\0') {
        *d = *src;
        d = d + 1;
        src = src + 1;
    }
    *d = '\0';
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n > 0 && *src != '\0') {
        *d = *src;
        d = d + 1;
        src = src + 1;
        n = n - 1;
    }
    /* Pad remaining bytes with NUL */
    while (n > 0) {
        *d = '\0';
        d = d + 1;
        n = n - 1;
    }
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d != '\0') {
        d = d + 1;
    }
    while (*src != '\0') {
        *d = *src;
        d = d + 1;
        src = src + 1;
    }
    *d = '\0';
    return dst;
}

char *strncat(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (*d != '\0') {
        d = d + 1;
    }
    while (n > 0 && *src != '\0') {
        *d = *src;
        d = d + 1;
        src = src + 1;
        n = n - 1;
    }
    *d = '\0';
    return dst;
}

char *strchr(const char *s, int c) {
    char ch = (char)c;
    while (*s != '\0') {
        if (*s == ch) return (char *)s;
        s = s + 1;
    }
    /* strchr also finds the terminating NUL */
    if (ch == '\0') return (char *)s;
    return NULL;
}

char *strrchr(const char *s, int c) {
    char ch = (char)c;
    const char *last = NULL;
    while (*s != '\0') {
        if (*s == ch) last = s;
        s = s + 1;
    }
    if (ch == '\0') return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    size_t nlen;
    if (*needle == '\0') return (char *)haystack;
    nlen = strlen(needle);
    while (*haystack != '\0') {
        if (*haystack == *needle && strncmp(haystack, needle, nlen) == 0) {
            return (char *)haystack;
        }
        haystack = haystack + 1;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    const char *a;
    int found;
    while (*s != '\0') {
        found = 0;
        a = accept;
        while (*a != '\0') {
            if (*s == *a) {
                found = 1;
                break;
            }
            a = a + 1;
        }
        if (!found) return count;
        count = count + 1;
        s = s + 1;
    }
    return count;
}

size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    const char *r;
    while (*s != '\0') {
        r = reject;
        while (*r != '\0') {
            if (*s == *r) return count;
            r = r + 1;
        }
        count = count + 1;
        s = s + 1;
    }
    return count;
}

char *strpbrk(const char *s, const char *accept) {
    const char *a;
    while (*s != '\0') {
        a = accept;
        while (*a != '\0') {
            if (*s == *a) return (char *)s;
            a = a + 1;
        }
        s = s + 1;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  memory functions                                                  */
/* ------------------------------------------------------------------ */

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n > 0) {
        *d = *s;
        d = d + 1;
        s = s + 1;
        n = n - 1;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        /* Copy forward */
        while (n > 0) {
            *d = *s;
            d = d + 1;
            s = s + 1;
            n = n - 1;
        }
    } else if (d > s) {
        /* Copy backward to handle overlap */
        d = d + n;
        s = s + n;
        while (n > 0) {
            d = d - 1;
            s = s - 1;
            *d = *s;
            n = n - 1;
        }
    }
    return dst;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    unsigned char val = (unsigned char)c;
    while (n > 0) {
        *p = val;
        p = p + 1;
        n = n - 1;
    }
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    while (n > 0) {
        if (*a != *b) return *a - *b;
        a = a + 1;
        b = b + 1;
        n = n - 1;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    unsigned char val = (unsigned char)c;
    while (n > 0) {
        if (*p == val) return (void *)p;
        p = p + 1;
        n = n - 1;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  stdlib.h                                                          */
/* ------------------------------------------------------------------ */

int abs(int n) {
    return n < 0 ? -n : n;
}

long labs(long n) {
    return n < 0 ? -n : n;
}

int isspace(int c);  /* forward declaration */

int atoi(const char *s) {
    int result = 0;
    int sign = 1;
    while (isspace(*s)) s = s + 1;
    if (*s == '-') {
        sign = -1;
        s = s + 1;
    } else if (*s == '+') {
        s = s + 1;
    }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s = s + 1;
    }
    return sign * result;
}

long atol(const char *s) {
    long result = 0;
    int sign = 1;
    while (isspace(*s)) s = s + 1;
    if (*s == '-') {
        sign = -1;
        s = s + 1;
    } else if (*s == '+') {
        s = s + 1;
    }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s = s + 1;
    }
    return sign * result;
}

int isdigit(int c);   /* forward declaration */
int isalpha(int c);   /* forward declaration */
int isxdigit(int c);  /* forward declaration */

/* Basic strtol supporting base 0 (auto-detect), 10, and 16. */
long strtol(const char *nptr, char **endptr, int base) {
    long result = 0;
    int sign = 1;
    int digit;

    /* Skip whitespace */
    while (isspace(*nptr)) nptr = nptr + 1;

    /* Handle sign */
    if (*nptr == '-') {
        sign = -1;
        nptr = nptr + 1;
    } else if (*nptr == '+') {
        nptr = nptr + 1;
    }

    /* Auto-detect base */
    if (base == 0) {
        if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
            base = 16;
            nptr = nptr + 2;
        } else if (*nptr == '0') {
            base = 8;
            nptr = nptr + 1;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
            nptr = nptr + 2;
        }
    }

    /* Convert digits */
    while (*nptr != '\0') {
        if (*nptr >= '0' && *nptr <= '9') {
            digit = *nptr - '0';
        } else if (*nptr >= 'a' && *nptr <= 'f') {
            digit = *nptr - 'a' + 10;
        } else if (*nptr >= 'A' && *nptr <= 'F') {
            digit = *nptr - 'A' + 10;
        } else {
            break;
        }
        if (digit >= base) break;
        result = result * base + digit;
        nptr = nptr + 1;
    }

    if (endptr != NULL) *endptr = (char *)nptr;
    return sign * result;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p != NULL) {
        memset(p, 0, total);
    }
    return p;
}

/* Simple LCG random number generator */
unsigned int rand_seed = 1;

void srand(unsigned int seed) {
    rand_seed = seed;
}

int rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16) & 0x7fff;
}

/* ------------------------------------------------------------------ */
/*  stdio.h helpers                                                   */
/* ------------------------------------------------------------------ */

/* Write decimal integer to buf, return number of chars written. */
int __sprint_int(char *buf, int val) {
    char tmp[12];  /* enough for -2147483648 + NUL */
    int i = 0;
    int len = 0;
    int neg = 0;
    unsigned int uval;

    if (val < 0) {
        neg = 1;
        /* Handle INT_MIN safely */
        uval = (unsigned int)(-(val + 1)) + 1;
    } else {
        uval = (unsigned int)val;
    }

    /* Generate digits in reverse */
    if (uval == 0) {
        tmp[i] = '0';
        i = i + 1;
    } else {
        while (uval > 0) {
            tmp[i] = '0' + (char)(uval % 10);
            uval = uval / 10;
            i = i + 1;
        }
    }

    if (neg) {
        buf[len] = '-';
        len = len + 1;
    }

    /* Copy digits in correct order */
    while (i > 0) {
        i = i - 1;
        buf[len] = tmp[i];
        len = len + 1;
    }

    buf[len] = '\0';
    return len;
}

/* Copy string to buf, return number of chars written. */
int __sprint_str(char *buf, const char *src) {
    int len = 0;
    if (src == NULL) {
        buf[0] = '(';
        buf[1] = 'n';
        buf[2] = 'u';
        buf[3] = 'l';
        buf[4] = 'l';
        buf[5] = ')';
        buf[6] = '\0';
        return 6;
    }
    while (*src != '\0') {
        buf[len] = *src;
        len = len + 1;
        src = src + 1;
    }
    buf[len] = '\0';
    return len;
}

/* Write unsigned hex to buf (no 0x prefix), return chars written. */
int __sprint_hex(char *buf, int val) {
    char tmp[9];  /* 8 hex digits + NUL */
    int i = 0;
    int len = 0;
    unsigned int uval = (unsigned int)val;
    int digit;

    if (uval == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    while (uval > 0) {
        digit = uval & 0xf;
        if (digit < 10) {
            tmp[i] = '0' + (char)digit;
        } else {
            tmp[i] = 'a' + (char)(digit - 10);
        }
        uval = uval >> 4;
        i = i + 1;
    }

    /* Copy digits in correct order */
    while (i > 0) {
        i = i - 1;
        buf[len] = tmp[i];
        len = len + 1;
    }

    buf[len] = '\0';
    return len;
}

/* Write single char to buf, return 1. */
int __sprint_char(char *buf, int ch) {
    buf[0] = (char)ch;
    buf[1] = '\0';
    return 1;
}

/* Print string followed by newline (uses host putchar). */
int puts(const char *s) {
    while (*s != '\0') {
        putchar(*s);
        s = s + 1;
    }
    putchar('\n');
    return 0;
}

/* ------------------------------------------------------------------ */
/*  ctype.h                                                           */
/* ------------------------------------------------------------------ */

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isalpha(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n'
        || c == '\r' || c == '\f' || c == '\v';
}

int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

int islower(int c) {
    return c >= 'a' && c <= 'z';
}

int isprint(int c) {
    return c >= 0x20 && c <= 0x7e;
}

int iscntrl(int c) {
    return (c >= 0 && c < 0x20) || c == 0x7f;
}

int ispunct(int c) {
    return isprint(c) && !isalnum(c) && c != ' ';
}

int isxdigit(int c) {
    return (c >= '0' && c <= '9')
        || (c >= 'A' && c <= 'F')
        || (c >= 'a' && c <= 'f');
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}
