// EXPECT_EXIT: 42

#define FOO 1
#define BAR 2

int main() {
    int result;
    result = 0;

    /* Test #ifdef / #endif */
#ifdef FOO
    result = result + 10;
#endif

    /* Test #ifndef */
#ifndef UNDEFINED_MACRO
    result = result + 10;
#endif

    /* Test #ifdef with #else */
#ifdef NONEXISTENT
    result = 0;
#else
    result = result + 10;
#endif

    /* Test #if with expression */
#if FOO == 1
    result = result + 5;
#endif

#if BAR > 5
    result = 0;
#endif

    /* Test nested conditionals */
#ifdef FOO
    #if BAR == 2
        result = result + 5;
    #endif
#endif

    /* Test #undef */
#undef FOO
#ifdef FOO
    result = 0;
#endif

    /* Test #if with defined() */
#if defined(BAR)
    result = result + 2;
#endif

#if !defined(FOO)
    /* FOO was undef'd — keep result */
#else
    result = 0;
#endif

    /* Test #elif */
#define VAL 3
#if VAL == 1
    result = 0;
#elif VAL == 2
    result = 0;
#elif VAL == 3
    result = result;
#else
    result = 0;
#endif

    /* Test __LINE__ (just verify it compiles and is positive) */
    if (__LINE__ <= 0) result = 0;

    /* Test __STDC__ */
#if __STDC__ != 1
    result = 0;
#endif

    /* Test #pragma (should be ignored) */
#pragma once

    /* result should be 10+10+10+5+5+2 = 42 */
    return result;
}
