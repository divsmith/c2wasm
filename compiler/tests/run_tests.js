#!/usr/bin/env node
/**
 * Node.js test runner for c2wasm compiler.
 *
 * Replaces run_tests.sh — uses wabt (npm) for WAT→WASM assembly
 * and a minimal WASI shim for execution. Only dependency: Node.js.
 *
 * Each .c file in programs/ can have:
 *   // EXPECT_EXIT: N   — expected exit code (default: 0)
 *   A matching .expected file — expected stdout
 */
'use strict';

var fs = require('fs');
var path = require('path');
var childProcess = require('child_process');
var wabtInit = require('wabt');
var wasi = require('../../tools/wasi-shim');

var SCRIPT_DIR = __dirname;
var COMPILER = path.resolve(SCRIPT_DIR, '../../build/c2wasm');
var PROG_DIR = path.join(SCRIPT_DIR, 'programs');

function compileC(sourceFile) {
  var result = childProcess.spawnSync(COMPILER, [], {
    input: fs.readFileSync(sourceFile),
    maxBuffer: 16 * 1024 * 1024
  });
  return {
    exitCode: result.status,
    stdout: result.stdout ? result.stdout.toString() : '',
    stderr: result.stderr ? result.stderr.toString() : ''
  };
}

function assembleWat(wabtInstance, watText, name) {
  try {
    var mod = wabtInstance.parseWat(name + '.wat', watText);
    var binary = mod.toBinary({ log: false, write_debug_names: false });
    mod.destroy();
    return { ok: true, bytes: binary.buffer };
  } catch (e) {
    return { ok: false, error: e.message || String(e) };
  }
}

function runWasm(wasmBytes, stdinBytes) {
  var mod = new WebAssembly.Module(wasmBytes);
  var ctx = wasi.createWasiContext(stdinBytes || new Uint8Array(0));
  var instance = new WebAssembly.Instance(mod, ctx.imports);
  ctx.setMemory(instance.exports.memory);

  try {
    instance.exports._start();
  } catch (e) {
    if (!ctx.isExit(e)) throw e;
  }

  return { exitCode: ctx.exitCode, stdout: ctx.stdout, stderr: ctx.stderr };
}

async function main() {
  var wabtInstance = await wabtInit();

  var files = fs.readdirSync(PROG_DIR)
    .filter(function (f) { return f.endsWith('.c'); })
    .sort();

  var pass = 0;
  var fail = 0;
  var total = files.length;

  for (var i = 0; i < files.length; i++) {
    var file = files[i];
    var name = file.replace(/\.c$/, '');
    var srcPath = path.join(PROG_DIR, file);

    // Parse expected exit code
    var firstLine = fs.readFileSync(srcPath, 'utf8').split('\n')[0];
    var exitMatch = firstLine.match(/EXPECT_EXIT:\s*(\d+)/);
    var expectedExit = exitMatch ? parseInt(exitMatch[1], 10) : 0;

    // Step 1: Compile C → WAT
    var compiled = compileC(srcPath);
    if (compiled.exitCode !== 0) {
      console.log('FAIL: ' + name + ' (compile error)');
      console.log(compiled.stderr.split('\n').slice(0, 5).join('\n'));
      fail++;
      continue;
    }

    // Step 2: Assemble WAT → WASM
    var assembled = assembleWat(wabtInstance, compiled.stdout, name);
    if (!assembled.ok) {
      console.log('FAIL: ' + name + ' (wat assembly error)');
      console.log(assembled.error.split('\n').slice(0, 5).join('\n'));
      fail++;
      continue;
    }

    // Step 3: Run WASM
    var result;
    try {
      result = runWasm(assembled.bytes);
    } catch (e) {
      console.log('FAIL: ' + name + ' (runtime error)');
      console.log((e.message || String(e)).split('\n').slice(0, 5).join('\n'));
      fail++;
      continue;
    }

    // Step 4: Check stdout if .expected file exists
    // Trim trailing newlines to match shell's $() behavior (strips trailing \n).
    var expectedPath = path.join(PROG_DIR, name + '.expected');
    if (fs.existsSync(expectedPath)) {
      var expectedOutput = fs.readFileSync(expectedPath, 'utf8').replace(/\n+$/, '');
      var actualOutput = result.stdout.replace(/\n+$/, '');
      if (actualOutput !== expectedOutput) {
        console.log('FAIL: ' + name + ' (output mismatch)');
        console.log('  expected: ' + JSON.stringify(expectedOutput.slice(0, 200)));
        console.log('  got:      ' + JSON.stringify(actualOutput.slice(0, 200)));
        fail++;
        continue;
      }
    }

    // Step 5: Check exit code
    if (result.exitCode === expectedExit) {
      console.log('PASS: ' + name + ' (exit ' + result.exitCode + ')');
      pass++;
    } else {
      console.log('FAIL: ' + name + ' (expected exit ' + expectedExit + ', got ' + result.exitCode + ')');
      fail++;
    }
  }

  console.log('');
  console.log('Results: ' + pass + '/' + total + ' passed, ' + fail + ' failed');
  process.exit(fail > 0 ? 1 : 0);
}

main().catch(function (err) {
  console.error('Test runner error:', err);
  process.exit(1);
});
