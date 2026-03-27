'use strict';

/**
 * Wraps the WASI-compiled c2wasm compiler (compiler.wasm).
 *
 * The compiler is built with wasi-sdk and runs via a minimal WASI shim.
 * It reads C source from stdin and writes WAT to stdout — the shim
 * wires those to in-memory buffers so no filesystem is needed.
 */
var CompilerAPI = (function () {
  var compiledModule = null;
  var loading = null;

  function init() {
    if (compiledModule) return Promise.resolve();
    if (loading) return loading;

    loading = fetch('compiler.wasm')
      .then(function (response) {
        if (!response.ok) throw new Error('compiler.wasm not found (HTTP ' + response.status + ').\nBuild it with: cd compiler && make wasm');
        return response.arrayBuffer();
      })
      .then(function (bytes) {
        return WebAssembly.compile(bytes);
      })
      .then(function (mod) {
        compiledModule = mod;
      });

    loading.catch(function () { loading = null; });
    return loading;
  }

  /**
   * Create a WASI import object that reads from `inputBytes` and captures
   * stdout/stderr into string arrays.
   */
  function createWasiImports(inputBytes, rawMode) {
    // State is stored on an object (not bare closure variables) to avoid
    // a V8 optimization issue where WASM-to-JS calls can miscompile
    // direct closure variable access.
    var state = {
      memory: null,
      inputPos: 0,
      stdoutChunks: [],
      stdoutByteChunks: [],
      stderrChunks: [],
      exitCode: 0,
      rawMode: rawMode || false
    };
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

        fd_close: function () { return 0; },
        fd_seek: function () { return 8; },
        fd_prestat_get: function () { return 8; },
        fd_prestat_dir_name: function () { return 8; },
        fd_fdstat_set_flags: function () { return 0; },
        path_open: function () { return 44; /* ENOENT — no filesystem in browser */ },
        fd_fdstat_get: function (fd, fdstat_ptr) {
          var view = new DataView(state.memory.buffer);
          view.setUint8(fdstat_ptr, fd <= 2 ? 2 : 4);
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
          view.setUint32(argc_ptr, 0, true);
          view.setUint32(argv_buf_size_ptr, 0, true);
          return 0;
        },
        args_get: function () { return 0; },
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

  function compile(cSource) {
    return init().then(function () {
      var inputBytes = new TextEncoder().encode(cSource);
      var ctx = createWasiImports(inputBytes);
      var instance = new WebAssembly.Instance(compiledModule, ctx.imports);
      ctx.setMemory(instance.exports.memory);

      try {
        instance.exports._start();
      } catch (e) {
        if (!ctx.isExit(e)) throw e;
        if (ctx.exitCode !== 0) {
          throw new Error(ctx.stderr || 'Compiler exited with code ' + ctx.exitCode);
        }
      }

      if (ctx.stderr && !ctx.stdout) {
        throw new Error(ctx.stderr);
      }

      return ctx.stdout;
    });
  }

  /**
   * Compile C source to WASM binary bytes.
   * Uses raw byte capture so binary output is not mangled by TextDecoder.
   * Returns a Promise<Uint8Array>.
   */
  function compileBinary(cSource) {
    return init().then(function () {
      var inputBytes = new TextEncoder().encode(cSource);
      var ctx = createWasiImports(inputBytes, true);
      var instance = new WebAssembly.Instance(compiledModule, ctx.imports);
      ctx.setMemory(instance.exports.memory);

      try {
        instance.exports._start();
      } catch (e) {
        if (!ctx.isExit(e)) throw e;
        if (ctx.exitCode !== 0) {
          throw new Error(ctx.stderr || 'Compiler exited with code ' + ctx.exitCode);
        }
      }

      var bytes = ctx.stdoutBytes;
      if (bytes.length === 0) {
        throw new Error(ctx.stderr || 'Compiler produced no output');
      }

      return bytes;
    });
  }

  return { compile: compile, compileBinary: compileBinary, init: init };
})();
