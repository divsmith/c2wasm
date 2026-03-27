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
   */
  function createWasiImports(inputBytes, rawMode, virtualFS, args) {
    var state = {
      memory: null,
      inputPos: 0,
      stdoutChunks: [],
      stdoutByteChunks: [],
      stderrChunks: [],
      exitCode: 0,
      rawMode: rawMode || false,
      args: args || ['c2wasm']
    };

    // Virtual filesystem state: fd → { content: Uint8Array, position: int }
    var openFiles = {};
    var nextFd = 4;

    var EXIT_TAG = { __wasi_exit: true };

    var imports = {
      wasi_snapshot_preview1: {
        proc_exit: function (code) {
          state.exitCode = code;
          throw EXIT_TAG;
        },

        fd_write: function (fd, iovs, iovs_len, nwritten_ptr) {
          var buf = state.memory.buffer;
          var view = new DataView(buf);
          var bytes = new Uint8Array(buf);
          var total = 0;

          if (fd !== 2 && state.rawMode) {
            for (var i = 0; i < iovs_len; i++) {
              var ptr = view.getUint32(iovs + i * 8, true);
              var len = view.getUint32(iovs + i * 8 + 4, true);
              state.stdoutByteChunks.push(bytes.slice(ptr, ptr + len));
              total += len;
            }
          } else {
            var chunks = fd === 2 ? state.stderrChunks : state.stdoutChunks;
            var decoder = new TextDecoder('utf-8', { fatal: false });

            for (var i = 0; i < iovs_len; i++) {
              var ptr = view.getUint32(iovs + i * 8, true);
              var len = view.getUint32(iovs + i * 8 + 4, true);
              var isLast = i === iovs_len - 1;
              chunks.push(decoder.decode(bytes.slice(ptr, ptr + len), { stream: !isLast }));
              total += len;
            }
          }

          view.setUint32(nwritten_ptr, total, true);
          return 0;
        },

        fd_read: function (fd, iovs, iovs_len, nread_ptr) {
          var buf = state.memory.buffer;
          var view = new DataView(buf);
          var bytes = new Uint8Array(buf);
          var totalRead = 0;

          // Virtual file read
          if (openFiles[fd]) {
            var vf = openFiles[fd];
            for (var i = 0; i < iovs_len; i++) {
              var iovPtr = view.getUint32(iovs + i * 8, true);
              var iovLen = view.getUint32(iovs + i * 8 + 4, true);
              var written = 0;
              while (written < iovLen && vf.position < vf.content.length) {
                bytes[iovPtr + written] = vf.content[vf.position++];
                written++;
                totalRead++;
              }
            }
            view.setUint32(nread_ptr, totalRead, true);
            return 0;
          }

          // stdin read
          for (var i = 0; i < iovs_len; i++) {
            var iovPtr = view.getUint32(iovs + i * 8, true);
            var iovLen = view.getUint32(iovs + i * 8 + 4, true);
            var written = 0;

            while (written < iovLen && state.inputPos < inputBytes.length) {
              bytes[iovPtr + written] = inputBytes[state.inputPos++];
              written++;
              totalRead++;
            }
          }

          view.setUint32(nread_ptr, totalRead, true);
          return 0;
        },

        fd_close: function (fd) {
          if (openFiles[fd]) delete openFiles[fd];
          return 0;
        },

        fd_seek: function () { return 8; },

        fd_prestat_get: function (fd, prestat_ptr) {
          // Advertise fd 3 as a preopened directory when we have a virtual FS
          if (fd === 3 && virtualFS) {
            var view = new DataView(state.memory.buffer);
            view.setUint8(prestat_ptr, 0); // __WASI_PREOPENTYPE_DIR
            view.setUint32(prestat_ptr + 4, 1, true); // path length (".")
            return 0;
          }
          return 8; // EBADF
        },

        fd_prestat_dir_name: function (fd, path_ptr, path_len) {
          if (fd === 3 && virtualFS) {
            var bytes = new Uint8Array(state.memory.buffer);
            bytes[path_ptr] = 0x2E; // '.'
            return 0;
          }
          return 8; // EBADF
        },

        fd_fdstat_set_flags: function () { return 0; },

        path_open: function (dirfd, dirflags, path_ptr, path_len, oflags, fs_rights_base, fs_rights_inheriting, fdflags, fd_ptr) {
          if (!virtualFS) return 44; // ENOENT

          var bytes = new Uint8Array(state.memory.buffer);
          var pathBytes = bytes.slice(path_ptr, path_ptr + path_len);
          var filename = new TextDecoder().decode(pathBytes);

          // Strip leading "./" if present
          if (filename.indexOf('./') === 0) filename = filename.substring(2);

          if (virtualFS[filename] === undefined) return 44; // ENOENT

          var fd = nextFd++;
          var encoded = new TextEncoder().encode(virtualFS[filename]);
          openFiles[fd] = { content: encoded, position: 0 };

          var view = new DataView(state.memory.buffer);
          view.setUint32(fd_ptr, fd, true);
          return 0;
        },

        fd_fdstat_get: function (fd, fdstat_ptr) {
          var view = new DataView(state.memory.buffer);
          if (openFiles[fd]) {
            view.setUint8(fdstat_ptr, 4); // REGULAR_FILE
            view.setUint16(fdstat_ptr + 2, 0, true);
            view.setBigUint64(fdstat_ptr + 8, BigInt('0x1FFFFFFF'), true);
            view.setBigUint64(fdstat_ptr + 16, BigInt('0x1FFFFFFF'), true);
            return 0;
          }
          view.setUint8(fdstat_ptr, fd <= 2 ? 2 : (fd === 3 && virtualFS ? 3 : 4));
          view.setUint16(fdstat_ptr + 2, 0, true);
          view.setBigUint64(fdstat_ptr + 8, BigInt('0x1FFFFFFF'), true);
          view.setBigUint64(fdstat_ptr + 16, BigInt('0x1FFFFFFF'), true);
          return 0;
        },
        environ_sizes_get: function (count_ptr, size_ptr) {
          var view = new DataView(state.memory.buffer);
          view.setUint32(count_ptr, 0, true);
          view.setUint32(size_ptr, 0, true);
          return 0;
        },
        environ_get: function () { return 0; },
        args_sizes_get: function (argc_ptr, argv_buf_size_ptr) {
          var view = new DataView(state.memory.buffer);
          view.setUint32(argc_ptr, state.args.length, true);
          var totalSize = 0;
          for (var i = 0; i < state.args.length; i++) {
            totalSize += new TextEncoder().encode(state.args[i]).length + 1;
          }
          view.setUint32(argv_buf_size_ptr, totalSize, true);
          return 0;
        },
        args_get: function (argv_ptr, argv_buf_ptr) {
          var view = new DataView(state.memory.buffer);
          var mem = new Uint8Array(state.memory.buffer);
          var bufOffset = argv_buf_ptr;
          for (var i = 0; i < state.args.length; i++) {
            view.setUint32(argv_ptr + i * 4, bufOffset, true);
            var encoded = new TextEncoder().encode(state.args[i]);
            mem.set(encoded, bufOffset);
            mem[bufOffset + encoded.length] = 0;
            bufOffset += encoded.length + 1;
          }
          return 0;
        },
        clock_time_get: function (id, precision, time_ptr) {
          var view = new DataView(state.memory.buffer);
          view.setBigUint64(time_ptr, BigInt(Date.now()) * BigInt(1000000), true);
          return 0;
        },
        random_get: function (buf_ptr, len) {
          var bytes = new Uint8Array(state.memory.buffer, buf_ptr, len);
          for (var i = 0; i < len; i++) bytes[i] = (Math.random() * 256) | 0;
          return 0;
        }
      }
    };

    return {
      imports: imports,
      setMemory: function (m) { state.memory = m; },
      get exitCode() { return state.exitCode; },
      get stdout() { return state.stdoutChunks.join(''); },
      get stdoutBytes() {
        var total = 0;
        var i;
        for (i = 0; i < state.stdoutByteChunks.length; i++)
          total += state.stdoutByteChunks[i].length;
        var result = new Uint8Array(total);
        var offset = 0;
        for (i = 0; i < state.stdoutByteChunks.length; i++) {
          result.set(state.stdoutByteChunks[i], offset);
          offset += state.stdoutByteChunks[i].length;
        }
        return result;
      },
      get stderr() { return state.stderrChunks.join(''); },
      isExit: function (e) { return e === EXIT_TAG; }
    };
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
    buildCompiler: buildCompiler,
    useReference: useReference,
    useCustom: useCustom,
    getMode: getMode,
    hasCustomCompiler: hasCustomCompiler,
    init: init
  };
})();
