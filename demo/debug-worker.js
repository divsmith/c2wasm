'use strict';

importScripts('wasi-harness.js');

// SharedArrayBuffer layout (Int32 view):
//   [0]: stdin handshake — worker sets to 0 before sleeping, main sets to 1 when input is ready
//   [1]: stdin byte count
//   [2]: debug pause slot — worker sets to 0 when paused, main writes signal to wake:
//          1 = proceed (step or continue; main thread decides pause logic)
//          3 = stop (throw to terminate execution)
// Byte view offset 8+: raw UTF-8 stdin bytes
//
// Run-mode fast-path is handled by the main thread: when in "continue" mode and the
// current line has no breakpoint, the main thread immediately writes signal=1 and
// notifies without waiting for user input. This keeps the worker protocol simple.

var SAB_INPUT_OFFSET = 8;
var DBG_PAUSE_SLOT   = 2;

var sab      = null;
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
      flushOutput();
      self.postMessage({ type: 'trace', line: line });

      // Park: signal paused, wait for main thread to release.
      // Main thread decides whether to immediately release (run mode, no breakpoint)
      // or wait for a user button click (step mode or breakpoint hit).
      Atomics.store(sabInt32, DBG_PAUSE_SLOT, 0);
      Atomics.wait(sabInt32, DBG_PAUSE_SLOT, 0);

      var signal = Atomics.load(sabInt32, DBG_PAUSE_SLOT);
      if (signal === 3) {
        throw new Error('__C2WASM_DEBUG_STOP__');
      }
      // signal === 1: proceed (step or continue — main thread decides)
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
