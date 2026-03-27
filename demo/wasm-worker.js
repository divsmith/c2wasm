'use strict';

// SharedArrayBuffer layout (Int32 view):
//   [0]: handshake signal — worker sets to 0 before sleeping, main sets to 1 when input is ready
//   [1]: byte count of the input written to the buffer
// Byte view offset 8+: raw UTF-8 input bytes (up to SAB_INPUT_SIZE bytes)

var SAB_INPUT_OFFSET = 8;

var sab = null;
var sabInt32 = null;
var memory = null;

// Local stdin ring: filled one line at a time from the SAB.
var stdinBuf = [];
var stdinPos = 0;

// Accumulates fd_write output until flushed to the main thread.
var pendingOutput = '';

self.onmessage = function (event) {
  var msg = event.data;
  if (msg.type !== 'run') return;

  sab = msg.sab;
  sabInt32 = new Int32Array(sab);

  try {
    runWasm(msg.wasmBytes);
  } catch (e) {
    self.postMessage({ type: 'error', message: e && e.message ? e.message : String(e) });
  }
};

function flushOutput() {
  if (pendingOutput.length > 0) {
    self.postMessage({ type: 'output', text: pendingOutput });
    pendingOutput = '';
  }
}

function requestInput() {
  // Flush any pending output FIRST so the prompt appears before the input cursor.
  flushOutput();
  // Signal the main thread and block this worker thread until input arrives.
  Atomics.store(sabInt32, 0, 0);
  self.postMessage({ type: 'needInput' });
  Atomics.wait(sabInt32, 0, 0);
  // Main thread has written input: read it.
  var byteCount = Atomics.load(sabInt32, 1);
  if (byteCount === 0) {
    return false; // EOF signaled
  }
  stdinBuf = Array.from(new Uint8Array(sab, SAB_INPUT_OFFSET, byteCount));
  stdinPos = 0;
  return true;
}

function runWasm(wasmBytes) {
  var importObject = {
    wasi_snapshot_preview1: {
      proc_exit: function (code) {
        throw { type: 'proc_exit', code: code };
      },

      fd_write: function (fd, iovs, iovs_len, nwritten_ptr) {
        var view = new DataView(memory.buffer);
        var bytes = new Uint8Array(memory.buffer);
        var total = 0;
        for (var i = 0; i < iovs_len; i++) {
          var ptr = view.getUint32(iovs + i * 8, true);
          var len = view.getUint32(iovs + i * 8 + 4, true);
          pendingOutput += new TextDecoder().decode(bytes.slice(ptr, ptr + len));
          total += len;
        }
        view.setUint32(nwritten_ptr, total, true);
        return 0;
      },

      fd_close: function () { return 0; },
      path_open: function () { return 44; /* ENOENT — no filesystem */ },

      fd_read: function (fd, iovs, iovs_len, nread_ptr) {
        var view = new DataView(memory.buffer);
        var bytes = new Uint8Array(memory.buffer);
        var totalRead = 0;

        for (var i = 0; i < iovs_len; i++) {
          var iovPtr = view.getUint32(iovs + i * 8, true);
          var iovLen = view.getUint32(iovs + i * 8 + 4, true);
          var written = 0;

          while (written < iovLen) {
            if (stdinPos >= stdinBuf.length) {
              if (!requestInput()) {
                // EOF
                view.setUint32(nread_ptr, totalRead, true);
                return 0;
              }
            }
            bytes[iovPtr + written] = stdinBuf[stdinPos++];
            written++;
            totalRead++;
          }
        }

        view.setUint32(nread_ptr, totalRead, true);
        return 0;
      }
    }
  };

  var module = new WebAssembly.Module(wasmBytes);
  var instance = new WebAssembly.Instance(module, importObject);
  memory = instance.exports.memory;

  try {
    instance.exports._start();
    flushOutput();
    self.postMessage({ type: 'exit', code: 0 });
  } catch (e) {
    if (e && e.type === 'proc_exit') {
      flushOutput();
      self.postMessage({ type: 'exit', code: e.code });
    } else {
      throw e;
    }
  }
}
