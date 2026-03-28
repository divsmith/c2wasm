'use strict';

/**
 * Wraps the WASI-compiled c2wasm compiler (compiler.wasm).
 *
 * The compiler is built with wasi-sdk and runs via a minimal WASI shim.
 * It reads C source from stdin and writes WAT/binary to stdout — the shim
 * wires those to in-memory buffers.
 *
 * Supports two compiler modes:
 *   - "reference": the shipped compiler.wasm (always available)
 *   - "custom": a compiler built from user-modified source (optional)
 */
var CompilerAPI = (function () {
  var referenceModule = null;
  var assemblerModule = null;
  var customModule = null;
  var currentMode = 'reference';
  var loading = null;

  function init() {
    if (referenceModule) return Promise.resolve();
    if (loading) return loading;

    loading = Promise.all([
      fetch('compiler.wasm').then(function (r) {
        if (!r.ok) throw new Error('compiler.wasm not found (HTTP ' + r.status + ').\nBuild it with: make wasm');
        return r.arrayBuffer();
      }).then(function (b) { return WebAssembly.compile(b); }),
      fetch('assembler.wasm').then(function (r) {
        if (!r.ok) throw new Error('assembler.wasm not found (HTTP ' + r.status + ').\nBuild it with: make wasm');
        return r.arrayBuffer();
      }).then(function (b) { return WebAssembly.compile(b); })
    ]).then(function (mods) {
      referenceModule = mods[0];
      assemblerModule = mods[1];
    });

    loading.catch(function () { loading = null; });
    return loading;
  }

  /** Return the currently active WebAssembly.Module. */
  function activeModule() {
    if (currentMode === 'custom' && customModule) return customModule;
    return referenceModule;
  }

  /**
   * Create a WASI import object that reads from `inputBytes` and captures
   * stdout/stderr into string arrays.
   *
   * When `virtualFS` is provided (a {filename: string} map), path_open/
   * fd_read/fd_close will serve files from it — enabling #include support
   * for in-browser compilation.
   *
   * Delegates to createWasiHarness() from wasi-harness.js.
   */
  function createWasiImports(inputBytes, rawMode, virtualFS, args) {
    return createWasiHarness({
      mode: 'buffer',
      inputBytes: inputBytes,
      rawMode: rawMode,
      virtualFS: virtualFS,
      args: args
    });
  }

  /** Run a compiler module with given source and options. */
  function runCompiler(mod, cSource, rawMode, virtualFS, args) {
    var inputBytes = new TextEncoder().encode(cSource);
    var ctx = createWasiImports(inputBytes, rawMode, virtualFS, args);
    var instance = new WebAssembly.Instance(mod, ctx.imports);
    ctx.setMemory(instance.exports.memory);

    try {
      instance.exports._start();
    } catch (e) {
      if (!ctx.isExit(e)) throw e;
      if (ctx.exitCode !== 0) {
        throw new Error(ctx.stderr || 'Compiler exited with code ' + ctx.exitCode);
      }
    }

    return ctx;
  }

  function compile(cSource) {
    return init().then(function () {
      var ctx = runCompiler(activeModule(), cSource, false, null);

      if (ctx.stderr && !ctx.stdout) {
        throw new Error(ctx.stderr);
      }

      return ctx.stdout;
    });
  }

  /**
   * Compile C source to WASM binary bytes.
   *
   * In reference mode: reference compiler with -b flag compiles directly.
   * In custom mode: custom compiler (WAT mode) produces WAT text, then
   * assembler.wasm converts WAT → WASM binary.
   *
   * Returns a Promise<Uint8Array>.
   */
  function compileBinary(cSource) {
    return init().then(function () {
      if (currentMode === 'custom' && customModule) {
        // Custom compiler produces WAT text (no -b flag)
        var watCtx = runCompiler(customModule, cSource, false, null);
        var wat = watCtx.stdout;
        if (!wat) {
          throw new Error(watCtx.stderr || 'Custom compiler produced no output');
        }
        // Assemble WAT → WASM binary via standalone assembler
        var asmCtx = runCompiler(assemblerModule, wat, true, null);
        var bytes = asmCtx.stdoutBytes;
        if (bytes.length === 0) {
          throw new Error(asmCtx.stderr || 'Assembler produced no output');
        }
        return bytes;
      }

      // Reference compiler produces binary directly via -b flag
      var ctx = runCompiler(referenceModule, cSource, true, null, ['c2wasm', '-b']);
      var bytes = ctx.stdoutBytes;
      if (bytes.length === 0) {
        throw new Error(ctx.stderr || 'Compiler produced no output');
      }
      return bytes;
    });
  }

  /**
   * Build a custom compiler from modified source files.
   * Uses the reference compiler to compile the source, producing a new
   * WASM binary that becomes the "custom" compiler.
   *
   * @param {Object} sourceFiles - Map of filename → source string
   * @returns {Promise<{size: number}>} - Result with compiled binary size
   */
  function buildCompiler(sourceFiles) {
    return init().then(function () {
      // Build custom compiler using reference compiler with -b flag.
      // The custom compiler will output WAT when used — compileBinary()
      // pipes custom WAT output through assembler.wasm for binary execution.
      var entry = sourceFiles['c2wasm.c'];
      if (!entry) throw new Error('c2wasm.c not found in source files');

      // Build virtual FS from all source files (excluding c2wasm.c which is stdin)
      var vfs = {};
      var keys = Object.keys(sourceFiles);
      for (var i = 0; i < keys.length; i++) {
        if (keys[i] !== 'c2wasm.c') {
          vfs[keys[i]] = sourceFiles[keys[i]];
        }
      }

      var ctx = runCompiler(referenceModule, entry, true, vfs, ['c2wasm', '-b']);

      var bytes = ctx.stdoutBytes;
      if (bytes.length === 0) {
        throw new Error(ctx.stderr || 'Compiler build produced no output');
      }

      return WebAssembly.compile(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength))
        .then(function (mod) {
          customModule = mod;
          currentMode = 'custom';
          return { size: bytes.length };
        });
    });
  }

  /**
   * Compile C source to debug-instrumented WASM binary.
   * Always uses the reference compiler with -D (debug trace) and -b (binary) flags.
   * Returns a Promise<Uint8Array>.
   */
  function compileDebug(cSource) {
    return init().then(function () {
      var ctx = runCompiler(referenceModule, cSource, true, null, ['c2wasm', '-D', '-b']);
      var bytes = ctx.stdoutBytes;
      if (bytes.length === 0) {
        throw new Error(ctx.stderr || 'Compiler produced no output');
      }
      return bytes;
    });
  }

  function useReference() { currentMode = 'reference'; }
  function useCustom() {
    if (!customModule) throw new Error('No custom compiler available');
    currentMode = 'custom';
  }
  function getMode() { return currentMode; }
  function hasCustomCompiler() { return customModule !== null; }

  return {
    compile: compile,
    compileBinary: compileBinary,
    compileDebug: compileDebug,
    buildCompiler: buildCompiler,
    useReference: useReference,
    useCustom: useCustom,
    getMode: getMode,
    hasCustomCompiler: hasCustomCompiler,
    init: init
  };
})();
