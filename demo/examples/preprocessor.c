// Preprocessor: #ifdef, #ifndef, #if, #elif, #undef, __LINE__

#define PLATFORM 2
#define DEBUG 1
#define VERSION 3

int main() {
    printf("=== Preprocessor Conditionals ===\n\n");

    /* #ifdef / #ifndef */
#ifdef DEBUG
    printf("[DEBUG] Debug mode is ON\n");
#endif

#ifndef RELEASE
    printf("[INFO] Release mode is OFF\n");
#endif

    /* #if with expressions and defined() */
#if PLATFORM == 1
    printf("Platform: Linux\n");
#elif PLATFORM == 2
    printf("Platform: WebAssembly\n");
#elif PLATFORM == 3
    printf("Platform: macOS\n");
#else
    printf("Platform: Unknown\n");
#endif

    /* Nested conditionals */
#if VERSION >= 2
    printf("Version %d features:\n", VERSION);
    #if defined(DEBUG)
        printf("  - verbose logging\n");
    #endif
    #if VERSION >= 3
        printf("  - new optimizer\n");
    #endif
#endif

    /* #undef */
#undef DEBUG
#ifdef DEBUG
    printf("BUG: should not print\n");
#else
    printf("DEBUG is now undefined\n");
#endif

    /* __LINE__ and __STDC__ */
    printf("Current line: %d\n", __LINE__);
#if __STDC__ == 1
    printf("Standard C compliant\n");
#endif

    /* #pragma is silently ignored */
#pragma once

    printf("\nAll preprocessor tests passed!\n");
    return 0;
}
