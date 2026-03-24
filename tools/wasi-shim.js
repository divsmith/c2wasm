/**
 * Minimal WASI shim for running c2wasm-compiled WASM in Node.js and browsers.
 *
 * Provides wasi_snapshot_preview1 imports:
 *   fd_read  — reads from a byte buffer (stdin)
 *   fd_write — captures output to a string (stdout/stderr)
 *   proc_exit — throws to signal exit
 *
 * Usage:
 *   var ctx = createWasiContext(stdinBytes);
 *   var instance = new WebAssembly.Instance(module, ctx.imports);
 *   ctx.setMemory(instance.exports.memory);
 *   try { instance.exports._start(); } catch (e) { if (!ctx.isExit(e)) throw e; }
 *   // ctx.exitCode, ctx.stdout, ctx.stderr
 */
'use strict';

function createWasiContext(stdinBytes) {
  // Shared state object — functions read memory from here.
  // Using a property on the imports object itself ensures V8 doesn't
  // optimize away the indirection when WASM calls into JS.
  var state = {
    memory: null,
    stdinBuf: stdinBytes || new Uint8Array(0),
    stdinPos: 0,
    stdoutChunks: [],
    stderrChunks: [],
    exitCode: 0
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
        var chunks = fd === 2 ? state.stderrChunks : state.stdoutChunks;
        var decoder = new TextDecoder('utf-8', { fatal: false });

        for (var i = 0; i < iovs_len; i++) {
          var ptr = view.getUint32(iovs + i * 8, true);
          var len = view.getUint32(iovs + i * 8 + 4, true);
          var slice = bytes.slice(ptr, ptr + len);
          var isLast = i === iovs_len - 1;
          chunks.push(decoder.decode(slice, { stream: !isLast }));
          total += len;
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

          while (written < iovLen && state.stdinPos < state.stdinBuf.length) {
            bytes[iovPtr + written] = state.stdinBuf[state.stdinPos++];
            written++;
            totalRead++;
          }
        }

        view.setUint32(nread_ptr, totalRead, true);
        return 0;
      },

      fd_close: function () { return 0; },
      fd_seek: function () { return 8; /* EBADF — unsupported */ },
      fd_prestat_get: function () { return 8; },
      fd_prestat_dir_name: function () { return 8; },
      fd_fdstat_get: function (fd, fdstat_ptr) {
        var view = new DataView(state.memory.buffer);
        // filetype = CHARACTER_DEVICE (2) so stdout is line-buffered in wasi-libc
        var filetype = (fd <= 2) ? 2 : 4;
        view.setUint8(fdstat_ptr, filetype);
        view.setUint16(fdstat_ptr + 2, 0, true);
        view.setBigUint64(fdstat_ptr + 8, BigInt("0x1FFFFFFF"), true);
        view.setBigUint64(fdstat_ptr + 16, BigInt("0x1FFFFFFF"), true);
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
    get stderr() { return state.stderrChunks.join(''); },
    isExit: function (e) { return e === EXIT_TAG; }
  };
}

// Export for Node.js (CommonJS) and browser (global)
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { createWasiContext: createWasiContext };
}
