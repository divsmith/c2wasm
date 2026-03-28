'use strict';

importScripts('wasi-harness.js');

// SharedArrayBuffer layout (Int32 view):
//   [0]: stdin handshake — worker sets to 0 before sleeping, main sets to 1 when input is ready
//   [1]: stdin byte count
//   [2]: debug pause slot — worker sets to 0 when paused, main writes signal to wake:
//          1 = step (advance one statement, then pause again)
//          2 = continue (run to end without pausing)
//          3 = stop (throw to terminate execution)
//   [3]: debug run mode — 0 = pause every statement, 1 = run to end (set by continue signal)
// Byte view offset 8+: raw UTF-8 stdin bytes

var SAB_INPUT_OFFSET = 8;
var DBG_PAUSE_SLOT   = 2;
var DBG_MODE_SLOT    = 3;

var sab = null;
var sabInt32 = null;
var pendingOutput = '';

self.onmessage = function (event) {
  var msg = event.data;
  if (msg.type !== 'run') return;

  sab = msg.sab;
  sabInt32 = new Int32Array(sab);

  try {
    runDebugWasm(msg.wasmBytes);
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

function runDebugWasm(wasmBytes) {
  var dbgImports = {
    trace: function (line) {
      // Fast path: in run-to-end mode, don't pause
      if (Atomics.load(sabInt32, DBG_MODE_SLOT) === 1) return;

      flushOutput();
      self.postMessage({ type: 'trace', line: line });

      // Park: signal paused, wait for main thread to release
      Atomics.store(sabInt32, DBG_PAUSE_SLOT, 0);
      Atomics.wait(sabInt32, DBG_PAUSE_SLOT, 0);

      var signal = Atomics.load(sabInt32, DBG_PAUSE_SLOT);
      if (signal === 3) {
        // Stop requested — throw a sentinel to unwind the WASM call stack
        throw new Error('__C2WASM_DEBUG_STOP__');
      }
      if (signal === 2) {
        // Continue — switch to run mode so future traces are skipped
        Atomics.store(sabInt32, DBG_MODE_SLOT, 1);
      }
      // signal === 1: step — stay in step mode (DBG_MODE_SLOT remains 0)
    }
  };

  var ctx = createWasiHarness({
    mode: 'interactive',
    sabInt32: sabInt32,
    sab: sab,
    sabInputOffset: SAB_INPUT_OFFSET,
    onBeforeBlock: flushOutput,
    postNeedInput: function () { self.postMessage({ type: 'needInput' }); },
    onOutput: function (text) { pendingOutput += text; },
    args: ['program'],
    extraImports: { dbg: dbgImports }
  });

  var module = new WebAssembly.Module(wasmBytes);
  var instance = new WebAssembly.Instance(module, ctx.imports);
  ctx.setMemory(instance.exports.memory);

  try {
    instance.exports._start();
    flushOutput();
    self.postMessage({ type: 'exit', code: ctx.exitCode });
  } catch (e) {
    if (ctx.isExit(e)) {
      flushOutput();
      self.postMessage({ type: 'exit', code: ctx.exitCode });
    } else if (e && e.message === '__C2WASM_DEBUG_STOP__') {
      flushOutput();
      self.postMessage({ type: 'stopped' });
    } else {
      throw e;
    }
  }
}
