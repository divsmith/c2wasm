#!/usr/bin/env node
/**
 * Self-hosting bootstrap validation for c2wasm.
 *
 * Replaces bootstrap.sh — uses wabt (npm) + WASI shim instead of
 * wat2wasm + wasmtime.
 *
 * 1. Native compiler compiles itself → s1.wat → s1.wasm
 * 2. WASM compiler (s1) compiles itself → s2.wat
 * 3. Verify s1.wat == s2.wat (fixed point)
 * 4. Run test suite with the WASM-compiled compiler
 */
'use strict';

var fs = require('fs');
var path = require('path');
var childProcess = require('child_process');
var wabtInit = require('wabt');
var wasi = require('./wasi-shim');

var ROOT = path.resolve(__dirname, '..');
var SRC = path.join(ROOT, 'compiler/src/c2wasm.c');
var BIN = path.join(ROOT, 'build/c2wasm');

function step(n, total, msg) {
  console.log('[' + n + '/' + total + '] ' + msg);
}

async function main() {
  var wabtInstance = await wabtInit();
  var sourceCode = fs.readFileSync(SRC);

  console.log('=== Self-hosting bootstrap ===');

  // Build native compiler
  step(1, 5, 'Building native compiler...');
  var build = childProcess.spawnSync('make', ['clean'], { cwd: path.join(ROOT, 'compiler'), stdio: 'pipe' });
  build = childProcess.spawnSync('make', [], { cwd: path.join(ROOT, 'compiler'), stdio: 'pipe' });
  if (build.status !== 0) {
    console.error('Build failed:', build.stderr.toString());
    process.exit(1);
  }

  // Stage 1: native compiler compiles itself
  step(2, 5, 'Stage 1: native compiler -> s1.wat');
  var s1 = childProcess.spawnSync(BIN, [], { input: sourceCode, maxBuffer: 64 * 1024 * 1024 });
  if (s1.status !== 0) {
    console.error('Stage 1 failed:', s1.stderr.toString());
    process.exit(1);
  }
  var s1Wat = s1.stdout.toString();
  var s1Lines = s1Wat.split('\n').length;
  console.log('       ' + s1Lines + ' lines of WAT');

  // Assemble s1.wat → s1.wasm
  var s1Module = wabtInstance.parseWat('s1.wat', s1Wat);
  var s1Binary = s1Module.toBinary({ log: false, write_debug_names: false });
  s1Module.destroy();
  var s1Wasm = s1Binary.buffer;

  // Stage 2: WASM compiler compiles itself
  step(3, 5, 'Stage 2: WASM compiler -> s2.wat');
  var mod = new WebAssembly.Module(s1Wasm);
  var ctx = wasi.createWasiContext(sourceCode);
  var instance = new WebAssembly.Instance(mod, ctx.imports);
  ctx.setMemory(instance.exports.memory);

  try {
    instance.exports._start();
  } catch (e) {
    if (!ctx.isExit(e)) throw e;
    if (ctx.exitCode !== 0) {
      console.error('Stage 2 failed with exit code ' + ctx.exitCode);
      console.error(ctx.stderr);
      process.exit(1);
    }
  }

  var s2Wat = ctx.stdout;
  var s2Lines = s2Wat.split('\n').length;
  console.log('       ' + s2Lines + ' lines of WAT');

  // Verify fixed point
  step(4, 5, 'Verifying fixed point (s1.wat == s2.wat)...');
  if (s1Wat === s2Wat) {
    console.log('       PASS: s1.wat and s2.wat are identical');
  } else {
    console.log('       FAIL: s1.wat and s2.wat differ!');
    // Find first difference
    var s1Lines = s1Wat.split('\n');
    var s2Lines = s2Wat.split('\n');
    for (var i = 0; i < Math.max(s1Lines.length, s2Lines.length); i++) {
      if (s1Lines[i] !== s2Lines[i]) {
        console.log('  Line ' + (i + 1) + ':');
        console.log('    s1: ' + (s1Lines[i] || '<EOF>'));
        console.log('    s2: ' + (s2Lines[i] || '<EOF>'));
        break;
      }
    }
    process.exit(1);
  }

  // Run test suite
  step(5, 5, 'Running test suite...');
  var testResult = childProcess.spawnSync('node', ['tests/run_tests.js'], {
    cwd: path.join(ROOT, 'compiler'),
    stdio: 'pipe',
    maxBuffer: 16 * 1024 * 1024
  });
  var testLines = testResult.stdout.toString().trim().split('\n');
  console.log(testLines[testLines.length - 1]);

  if (testResult.status !== 0) {
    console.error('\nTest suite failed!');
    process.exit(1);
  }

  console.log('');
  console.log('=== Bootstrap successful! ===');
}

main().catch(function (err) {
  console.error('Bootstrap error:', err);
  process.exit(1);
});
