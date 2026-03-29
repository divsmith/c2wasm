## Roadmap

1. [x] Simplify compiler code with newly implemented features
2. [x] Remove external dependencies (emcc, etc). Directly emit WASM binary.
3. [x] Demo console inputs
4. [x] Live compiler updates - hack on the compiler that's actually running in the demo
    * Need a way to "revert" back when the compiler is broken.
    * Maybe an option to select running either the "reference" known good compiler binary or toggle to the one who's source code has been changed in the browser.
    * The "reference" compiler would be responsible for compiling the hacked version, which could then in turn be responsible for compiling the user's test or example code (hello world, etc) that then runs in the browser
5. [x] Real free() implementation
6. [x] More data types
7. [ ] Full C89 compatibility
8. [x] libc available in browser demo
9. [x] Remove WAT output from demo
10. [x] pull src and test directories out of compilers to project root
    * The demo should be an accessory to the main compiler, not an equivalent level
11. [ ] debugger / live code execution visualizer
12. [ ] use wasm f64 if not already
13. [x] implement C features that would make the self-hosting compiler easier to implement itself, then iteratively do so
14. [x] break compiler up into multiple components instead of one massive file. maintain self-hosting requirement and use newly supported language features to simplify where possible.
15. [x] build out full compiler toolchain, i.e. source -> wat, and wat -> wasm, instead of separate source -> wat and source -> wasm / binary modes
16. [ ] Add a linker, separate out the libc methods within the compiler to a real version of libc that's linked and included
17. [x] real random source from browser
18. [x] load example programs into virtual FS instead of embedding in demo page JS
19. [x] Compiler to depend on only WASI runtime, shim WASI into demo as a compiler harness

## Recommended Enhancements

### Language Completeness (C89)

20. [ ] `long long` / `i64` mapping — Support 64-bit integers via WASM `i64` opcodes, unlocking broader compatibility
21. [ ] Bit fields in structs — `int x : 4` syntax for memory-efficient flag storage
22. [ ] Designated initializers — `struct Foo f = {.x = 1, .y = 2}` for clearer initialization
23. [ ] Compound literals — `(struct Point){1, 2}` expressions for function arguments
24. [ ] Extended `printf`/`sprintf` formats — Add `%u`, `%o`, `%p`, `%ld`, `%e`, `%g` specifiers
25. [ ] Math library functions — `sin`, `cos`, `sqrt`, `pow`, `floor`, `ceil`, `fabs`, `fmod` (map to native WASM f64 ops)
26. [ ] Extended string parsing — `strtod`, `strtoul`, `strtof` (complement existing `strtol`)

### WASM Code Quality

27. [ ] `f32` for `float` type — Use WASM `f32` instead of `f64` for `float` (more correct, smaller binaries)
28. [ ] Constant folding — Evaluate constant expressions at compile time (`2 + 3` → `i32.const 5`)
29. [ ] Dead code elimination — Strip unreachable functions from WASM output
30. [ ] Binary mode self-hosting fix — Resolve runtime crash in binary-mode compilation of the compiler itself

### Developer Experience

31. [ ] Warning system — Emit warnings for implicit declarations, unused variables, missing returns, switch fallthrough
32. [ ] Rich error messages — Show context (`^~~~`) and suggest fixes (e.g., "did you mean `==`?")
33. [ ] `#pragma once` support — Simple preprocessor directive to prevent multiple inclusions
34. [ ] Compiler introspection flags — `--dump-tokens`, `--dump-ast` for debugging and education

### Demo & Browser Features

35. [ ] Share via URL — Encode program as URL fragment or Gist link for easy sharing
36. [ ] Named function exports — Allow `__export__` annotations to expose functions as WASM exports for JS interaction
37. [ ] JS→C function imports — Define shim layer so C code can call JS functions (canvas, DOM, etc.)
38. [ ] Multi-file editing — Tabs for multiple `.c`/`.h` files reflecting `#include` structure
39. [ ] Compile-on-save with debounce — Auto-recompile after brief pause for instant feedback

### Testing & Tooling

40. [ ] Fuzz testing — Ensure compiler exits cleanly on malformed input (no crashes/hangs)
41. [ ] Benchmark suite — Micro-benchmarks (matrix multiply, sorting, string ops) to track performance
42. [ ] Differential testing vs GCC — Compare output on supported subset to catch miscompilations

### Educational Features

43. [ ] AST visualizer — Interactive tree view of parsed abstract syntax tree in demo
44. [ ] WAT annotation mode — Emit comments linking WAT instructions back to source lines
45. [ ] Source-linked debugger — Highlight C source line as WASM executes, bridging abstraction gap