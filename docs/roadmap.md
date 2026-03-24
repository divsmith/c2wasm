## Roadmap

1. [x] Simplify compiler code with newly implemented features
2. [ ] Remove external dependencies (emcc, etc). Directly emit WASM binary.
3. [x] Demo console inputs
4. [ ] Live compiler updates - hack on the compiler that's actually running in the demo
    * Need a way to "revert" back when the compiler is broken.
    * Maybe an option to select running either the "reference" known good compiler binary or toggle to the one who's source code has been changed in the browser.
    * The "reference" compiler would be responsible for compiling the hacked version, which could then in turn be responsible for compiling the user's test or example code (hello world, etc) that then runs in the browser
5. [ ] Real free() implementation
6. [ ] More data types
7. [ ] Full C89 compatibility
8. [ ] libc available in browser demo