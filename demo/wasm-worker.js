'use strict';

importScripts('wasi-harness.js');

// SharedArrayBuffer layout (Int32 view):
//   [0]: handshake signal — worker sets to 0 before sleeping, main sets to 1 when input is ready
//   [1]: byte count of the input written to the buffer
// Byte view offset 8+: raw UTF-8 input bytes (up to SAB_INPUT_SIZE bytes)

var SAB_INPUT_OFFSET = 8;

var sab = null;
var sabInt32 = null;

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

function runWasm(wasmBytes) {
  var ctx = createWasiHarness({
    mode: 'interactive',
    sabInt32: sabInt32,
    sab: sab,
    sabInputOffset: SAB_INPUT_OFFSET,
    onBeforeBlock: flushOutput,
    postNeedInput: function () { self.postMessage({ type: 'needInput' }); },
    onOutput: function (text) { pendingOutput += text; },
    args: ['program']
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
    } else {
      throw e;
    }
  }
}
