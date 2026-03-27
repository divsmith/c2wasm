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
11. [ ] debugger / live code visualizer
12. [ ] use wasm f64 if not already
13. [x] implement C features that would make the self-hosting compiler easier to implement itself, then iteratively do so
14. [x] break compiler up into multiple components instead of one massive file. maintain self-hosting requirement and use newly supported language features to simplify where possible.
15. [x] build out full compiler toolchain, i.e. source -> wat, and wat -> wasm, instead of separate source -> wat and source -> wasm / binary modes
16. [ ] Add a linker, separate out the libc methods within the compiler to a real version of libc that's linked and included
17. [ ] real random source from browser