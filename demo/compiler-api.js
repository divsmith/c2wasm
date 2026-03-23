'use strict';

/**
 * Wraps the Emscripten-compiled c2wasm compiler.
 * Expects compiler.js (produced by `make wasm`) to define C2WasmModule globally.
 *
 * We pre-fetch compiler.wasm ourselves and pass it as `wasmBinary` so Emscripten
 * never needs to fetch the file internally. This avoids the "both async and sync
 * fetching of the wasm failed" error that occurs when the page is opened via
 * file:// or the internal path resolution goes wrong.
 */
var CompilerAPI = (function () {
  var moduleFactory = null;
  var wasmBinaryData = null;
  var loading = null;

  function init() {
    if (moduleFactory && wasmBinaryData) return Promise.resolve();
    if (loading) return loading;

    var scriptPromise = new Promise(function (resolve, reject) {
      if (typeof C2WasmModule === 'function') {
        moduleFactory = C2WasmModule;
        resolve();
        return;
      }

      var script = document.createElement('script');
      script.src = 'compiler.js';
      script.onload = function () {
        if (typeof C2WasmModule === 'function') {
          moduleFactory = C2WasmModule;
          resolve();
        } else {
          reject(new Error('compiler.js loaded but C2WasmModule not found'));
        }
      };
      script.onerror = function () {
        reject(new Error(
          'compiler.js not found.\n' +
          'Build it with Emscripten:\n' +
          '  cd compiler && make wasm'
        ));
      };
      document.head.appendChild(script);
    });

    var wasmPromise = fetch('compiler.wasm')
      .then(function (res) {
        if (!res.ok) {
          throw new Error(
            'compiler.wasm not found (HTTP ' + res.status + ').\n' +
            'Build it with Emscripten:\n' +
            '  cd compiler && make wasm'
          );
        }
        return res.arrayBuffer();
      })
      .then(function (buf) {
        wasmBinaryData = new Uint8Array(buf);
      })
      .catch(function (err) {
        if (err.message && err.message.indexOf('compiler.wasm') !== -1) {
          throw err;
        }
        throw new Error(
          'Failed to load compiler.wasm.\n' +
          'The demo must be served over HTTP, not opened as a local file.\n' +
          'Run:\n' +
          '  cd compiler && make serve\n' +
          'Then open http://localhost:8080/\n' +
          '(original error: ' + err.message + ')'
        );
      });

    loading = Promise.all([scriptPromise, wasmPromise]).then(function () {});
    loading.catch(function () { loading = null; });
    return loading;
  }

  function compile(cSource) {
    return init().then(function () {
      var inputPos = 0;
      var watOutput = '';
      var errorOutput = '';

      return moduleFactory({
        wasmBinary: wasmBinaryData,
        noInitialRun: true,
        stdin: function () {
          if (inputPos < cSource.length) {
            return cSource.charCodeAt(inputPos++);
          }
          return null;
        },
        print: function (text) {
          watOutput += text + '\n';
        },
        printErr: function (text) {
          errorOutput += text + '\n';
        }
      }).then(function (module) {
        try {
          module.callMain([]);
        } catch (e) {
          if (e && e.name === 'ExitStatus' && e.status === 0) {
            // Normal exit
          } else if (e && (e.name === 'ExitStatus' || e.status !== undefined)) {
            throw new Error(errorOutput || 'Compiler exited with code ' + (e.status || 1));
          } else {
            throw e;
          }
        }

        if (errorOutput && !watOutput) {
          throw new Error(errorOutput);
        }

        return watOutput;
      });
    });
  }

  return { compile: compile, init: init };
})();
