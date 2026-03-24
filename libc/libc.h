#ifndef LIBC_H
#define LIBC_H

/* Types */
typedef unsigned int size_t;
typedef int ptrdiff_t;

/* A NULL define */
#define NULL ((void *)0)

/* string.h */
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strpbrk(const char *s, const char *accept);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);

/* stdlib.h */
int atoi(const char *s);
long atol(const char *s);
long strtol(const char *nptr, char **endptr, int base);
int abs(int n);
long labs(long n);
void *calloc(size_t nmemb, size_t size);
int rand(void);
void srand(unsigned int seed);

/* stdio.h helpers (no variadic support in c2wasm) */
int __sprint_int(char *buf, int val);
int __sprint_str(char *buf, const char *src);
int __sprint_hex(char *buf, int val);
int __sprint_char(char *buf, int ch);
int puts(const char *s);

/* ctype.h */
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isspace(int c);
int isupper(int c);
int islower(int c);
int isprint(int c);
int iscntrl(int c);
int ispunct(int c);
int isxdigit(int c);
int toupper(int c);
int tolower(int c);

#endif
