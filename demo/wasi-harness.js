'use strict';

/**
 * wasi-harness.js — Shared WASI runtime harness
 *
 * Implements the full wasi_snapshot_preview1 interface used by both the
 * compiler (compiler-api.js) and user-compiled programs (wasm-worker.js).
 *
 * Usage:
 *   var ctx = createWasiHarness(config);
 *   var instance = new WebAssembly.Instance(module, ctx.imports);
 *   ctx.setMemory(instance.exports.memory);
 *   instance.exports._start();
 *   // ctx.stdout, ctx.exitCode, etc.
 *
 * Two stdin modes:
 *
 *   Buffer mode (compiler — main thread):
 *     mode: 'buffer'
 *     inputBytes: Uint8Array   — pre-loaded stdin content
 *     rawMode: bool            — capture stdout as raw bytes for binary output
 *     virtualFS: {name: str}   — virtual filesystem for #include support
 *     args: string[]           — argv
 *
 *   Interactive mode (user programs — Web Worker only):
 *     mode: 'interactive'
 *     sabInt32: Int32Array     — Int32 view of SharedArrayBuffer (layout: [signal, byteCount])
 *     sab: SharedArrayBuffer   — shared memory for stdin transfer
 *     sabInputOffset: number   — byte offset within sab where input bytes start
 *     onBeforeBlock: function  — called before Atomics.wait (flush pending output)
 *     postNeedInput: function  — signals main thread to provide more input
 *     onOutput: function(text) — receives each text chunk written to stdout/stderr
 *     args: string[]
 */
function createWasiHarness(config) {
  var mode = config.mode || 'buffer';
  var args = config.args || (mode === 'buffer' ? ['c2wasm'] : ['program']);
  var virtualFS = config.virtualFS || null;
  var rawMode = config.rawMode || false;

  var state = {
    memory: null,
    exitCode: 0,
    stdoutChunks: [],
    stdoutByteChunks: [],
    stderrChunks: [],
    inputPos: 0,
    inputBytes: config.inputBytes || new Uint8Array(0),
    stdinBuf: [],
    stdinPos: 0
  };

  var openFiles = {};
  var nextFd = 4;

  var EXIT_TAG = { __wasi_exit: true };

  function writeText(fd, text) {
    if (mode === 'interactive' && config.onOutput) {
      config.onOutput(text);
    } else {
      (fd === 2 ? state.stderrChunks : state.stdoutChunks).push(text);
    }
  }

  function requestMoreInput() {
    if (config.onBeforeBlock) config.onBeforeBlock();
    Atomics.store(config.sabInt32, 0, 0);
    config.postNeedInput();
    Atomics.wait(config.sabInt32, 0, 0);
    var byteCount = Atomics.load(config.sabInt32, 1);
    if (byteCount === 0) return false;
    state.stdinBuf = Array.from(new Uint8Array(config.sab, config.sabInputOffset, byteCount));
    state.stdinPos = 0;
    return true;
  }

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

        if (fd !== 2 && rawMode) {
          for (var i = 0; i < iovs_len; i++) {
            var ptr = view.getUint32(iovs + i * 8, true);
            var len = view.getUint32(iovs + i * 8 + 4, true);
            state.stdoutByteChunks.push(bytes.slice(ptr, ptr + len));
            total += len;
          }
        } else {
          var decoder = new TextDecoder('utf-8', { fatal: false });
          for (var i = 0; i < iovs_len; i++) {
            var ptr = view.getUint32(iovs + i * 8, true);
            var len = view.getUint32(iovs + i * 8 + 4, true);
            var isLast = i === iovs_len - 1;
            writeText(fd, decoder.decode(bytes.slice(ptr, ptr + len), { stream: !isLast }));
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
        var i, iovPtr, iovLen, written;

        if (openFiles[fd]) {
          var vf = openFiles[fd];
          for (i = 0; i < iovs_len; i++) {
            iovPtr = view.getUint32(iovs + i * 8, true);
            iovLen = view.getUint32(iovs + i * 8 + 4, true);
            written = 0;
            while (written < iovLen && vf.position < vf.content.length) {
              bytes[iovPtr + written] = vf.content[vf.position++];
              written++;
              totalRead++;
            }
          }
          view.setUint32(nread_ptr, totalRead, true);
          return 0;
        }

        if (mode === 'interactive') {
          for (i = 0; i < iovs_len; i++) {
            iovPtr = view.getUint32(iovs + i * 8, true);
            iovLen = view.getUint32(iovs + i * 8 + 4, true);
            written = 0;
            while (written < iovLen) {
              if (state.stdinPos >= state.stdinBuf.length) {
                if (!requestMoreInput()) {
                  view.setUint32(nread_ptr, totalRead, true);
                  return 0;
                }
              }
              bytes[iovPtr + written] = state.stdinBuf[state.stdinPos++];
              written++;
              totalRead++;
            }
          }
        } else {
          for (i = 0; i < iovs_len; i++) {
            iovPtr = view.getUint32(iovs + i * 8, true);
            iovLen = view.getUint32(iovs + i * 8 + 4, true);
            written = 0;
            while (written < iovLen && state.inputPos < state.inputBytes.length) {
              bytes[iovPtr + written] = state.inputBytes[state.inputPos++];
              written++;
              totalRead++;
            }
          }
        }

        view.setUint32(nread_ptr, totalRead, true);
        return 0;
      },

      fd_close: function (fd) {
        if (openFiles[fd]) delete openFiles[fd];
        return 0;
      },

      fd_seek: function () { return 8; /* ESPIPE */ },

      fd_prestat_get: function (fd, prestat_ptr) {
        if (fd === 3 && virtualFS) {
          var view = new DataView(state.memory.buffer);
          view.setUint8(prestat_ptr, 0);       // __WASI_PREOPENTYPE_DIR
          view.setUint32(prestat_ptr + 4, 1, true); // path length "."
          return 0;
        }
        return 8; // EBADF
      },

      fd_prestat_dir_name: function (fd, path_ptr) {
        if (fd === 3 && virtualFS) {
          new Uint8Array(state.memory.buffer)[path_ptr] = 0x2E; // '.'
          return 0;
        }
        return 8; // EBADF
      },

      fd_fdstat_set_flags: function () { return 0; },

      path_open: function (dirfd, dirflags, path_ptr, path_len, oflags, fs_rights_base, fs_rights_inheriting, fdflags, fd_ptr) {
        if (!virtualFS) return 44; // ENOENT

        var bytes = new Uint8Array(state.memory.buffer);
        var filename = new TextDecoder().decode(bytes.slice(path_ptr, path_ptr + path_len));
        if (filename.indexOf('./') === 0) filename = filename.substring(2);
        if (virtualFS[filename] === undefined) return 44; // ENOENT

        var fd = nextFd++;
        openFiles[fd] = { content: new TextEncoder().encode(virtualFS[filename]), position: 0 };
        new DataView(state.memory.buffer).setUint32(fd_ptr, fd, true);
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
        view.setUint32(argc_ptr, args.length, true);
        var totalSize = 0;
        for (var i = 0; i < args.length; i++) {
          totalSize += new TextEncoder().encode(args[i]).length + 1;
        }
        view.setUint32(argv_buf_size_ptr, totalSize, true);
        return 0;
      },

      args_get: function (argv_ptr, argv_buf_ptr) {
        var view = new DataView(state.memory.buffer);
        var mem = new Uint8Array(state.memory.buffer);
        var bufOffset = argv_buf_ptr;
        for (var i = 0; i < args.length; i++) {
          view.setUint32(argv_ptr + i * 4, bufOffset, true);
          var encoded = new TextEncoder().encode(args[i]);
          mem.set(encoded, bufOffset);
          mem[bufOffset + encoded.length] = 0;
          bufOffset += encoded.length + 1;
        }
        return 0;
      },

      clock_time_get: function (id, precision, time_ptr) {
        new DataView(state.memory.buffer).setBigUint64(
          time_ptr, BigInt(Date.now()) * BigInt(1000000), true
        );
        return 0;
      },

      random_get: function (buf_ptr, len) {
        var bytes = new Uint8Array(state.memory.buffer, buf_ptr, len);
        if (typeof crypto !== 'undefined' && crypto.getRandomValues) {
          crypto.getRandomValues(bytes);
        } else {
          for (var i = 0; i < len; i++) bytes[i] = (Math.random() * 256) | 0;
        }
        return 0;
      }

    }
  };

  if (config.extraImports) {
    var extraKeys = Object.keys(config.extraImports);
    for (var ei = 0; ei < extraKeys.length; ei++) {
      imports[extraKeys[ei]] = config.extraImports[extraKeys[ei]];
    }
  }

  return {
    imports: imports,
    setMemory: function (m) { state.memory = m; },
    get exitCode() { return state.exitCode; },
    get stdout() { return state.stdoutChunks.join(''); },
    get stdoutBytes() {
      var total = 0;
      var i;
      for (i = 0; i < state.stdoutByteChunks.length; i++) total += state.stdoutByteChunks[i].length;
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
