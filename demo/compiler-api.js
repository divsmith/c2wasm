'use strict';

/**
 * Wraps the Emscripten-compiled c2wasm compiler.
 * Expects compiler.js (produced by `make wasm`) to define C2WasmModule globally.
 */
var CompilerAPI = (function () {
  var moduleFactory = null;
  var loading = null;

  function init() {
    if (moduleFactory) return Promise.resolve();
    if (loading) return loading;

    loading = new Promise(function (resolve, reject) {
      if (typeof C2WasmModule === 'function') {
        moduleFactory = C2WasmModule;
        resolve();
        return;
      }

      // Try loading compiler.js dynamically
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

    loading.catch(function () { loading = null; });
    return loading;
  }

  function compile(cSource) {
    return init().then(function () {
      var inputPos = 0;
      var watOutput = '';
      var errorOutput = '';

      return moduleFactory({
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
