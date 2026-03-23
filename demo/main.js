'use strict';

(function () {
  // ── Example programs (embedded to avoid file:// fetch issues) ──

  var EXAMPLES = {
    hello:
      '// Hello World\n' +
      'int main() {\n' +
      '    printf("Hello, World!\\n");\n' +
      '    return 0;\n' +
      '}\n',

    fibonacci:
      '// Fibonacci sequence\n' +
      'int fib(int n) {\n' +
      '    if (n <= 1) {\n' +
      '        return n;\n' +
      '    }\n' +
      '    return fib(n - 1) + fib(n - 2);\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    int i;\n' +
      '    for (i = 0; i <= 10; i = i + 1) {\n' +
      '        printf("fib(%d) = %d\\n", i, fib(i));\n' +
      '    }\n' +
      '    return 0;\n' +
      '}\n',

    linked_list:
      '// Linked list traversal\n' +
      'struct Node {\n' +
      '    int val;\n' +
      '    int next;\n' +
      '};\n' +
      '\n' +
      'int main() {\n' +
      '    struct Node *a = malloc(sizeof(struct Node));\n' +
      '    struct Node *b = malloc(sizeof(struct Node));\n' +
      '    struct Node *c = malloc(sizeof(struct Node));\n' +
      '    a->val = 10; a->next = b;\n' +
      '    b->val = 20; b->next = c;\n' +
      '    c->val = 30; c->next = 0;\n' +
      '\n' +
      '    int *cur = a;\n' +
      '    int sum = 0;\n' +
      '    while (cur != 0) {\n' +
      '        struct Node *n = cur;\n' +
      '        printf("%d -> ", n->val);\n' +
      '        sum = sum + n->val;\n' +
      '        cur = n->next;\n' +
      '    }\n' +
      '    printf("NULL\\n");\n' +
      '    printf("sum = %d\\n", sum);\n' +
      '    return 0;\n' +
      '}\n',

    sort:
      '// Bubble sort\n' +
      'void swap(int *arr, int i, int j) {\n' +
      '    int tmp = arr[i];\n' +
      '    arr[i] = arr[j];\n' +
      '    arr[j] = tmp;\n' +
      '}\n' +
      '\n' +
      'void bubble_sort(int *arr, int n) {\n' +
      '    int i;\n' +
      '    int j;\n' +
      '    for (i = 0; i < n - 1; i = i + 1) {\n' +
      '        for (j = 0; j < n - 1 - i; j = j + 1) {\n' +
      '            if (arr[j] > arr[j + 1]) {\n' +
      '                swap(arr, j, j + 1);\n' +
      '            }\n' +
      '        }\n' +
      '    }\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    int *arr = malloc(28);\n' +
      '    int i;\n' +
      '    arr[0] = 64;\n' +
      '    arr[1] = 34;\n' +
      '    arr[2] = 25;\n' +
      '    arr[3] = 12;\n' +
      '    arr[4] = 22;\n' +
      '    arr[5] = 11;\n' +
      '    arr[6] = 90;\n' +
      '\n' +
      '    printf("Before: ");\n' +
      '    for (i = 0; i < 7; i = i + 1) {\n' +
      '        printf("%d ", arr[i]);\n' +
      '    }\n' +
      '    printf("\\n");\n' +
      '\n' +
      '    bubble_sort(arr, 7);\n' +
      '\n' +
      '    printf("After:  ");\n' +
      '    for (i = 0; i < 7; i = i + 1) {\n' +
      '        printf("%d ", arr[i]);\n' +
      '    }\n' +
      '    printf("\\n");\n' +
      '    return 0;\n' +
      '}\n'
  };

  // ── DOM references ──

  var editorContainer = document.getElementById('editor-container');
  var exampleSelect = document.getElementById('example-select');
  var runBtn = document.getElementById('run-btn');
  var statusEl = document.getElementById('status');
  var consoleOutput = document.getElementById('console-output');
  var watOutput = document.getElementById('wat-output');
  var tabs = document.querySelectorAll('.tab');
  var resizeHandle = document.getElementById('resize-handle');

  var editor = null;
  var wabtInstance = null;
  var isRunning = false;
  var idleTimerId = null;

  // ── Tab switching ──

  function activateTab(name) {
    for (var i = 0; i < tabs.length; i++) {
      var t = tabs[i];
      if (t.dataset.tab === name) {
        t.classList.add('active');
      } else {
        t.classList.remove('active');
      }
    }
    consoleOutput.classList.toggle('hidden', name !== 'console');
    watOutput.classList.toggle('hidden', name !== 'wat');
  }

  for (var i = 0; i < tabs.length; i++) {
    (function (tab) {
      tab.addEventListener('click', function () {
        activateTab(tab.dataset.tab);
      });
    })(tabs[i]);
  }

  // ── Example selector ──

  exampleSelect.addEventListener('change', function () {
    var code = EXAMPLES[exampleSelect.value];
    if (code && editor) {
      editor.setValue(code);
      monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
    }
  });

  // ── Panel resize ──

  (function () {
    var dragging = false;
    var editorPanel = document.querySelector('.editor-panel');

    resizeHandle.addEventListener('mousedown', function (e) {
      e.preventDefault();
      dragging = true;
      resizeHandle.classList.add('active');
      document.body.style.cursor = 'col-resize';
      document.body.style.userSelect = 'none';
    });

    document.addEventListener('mousemove', function (e) {
      if (!dragging) return;
      var mainRect = editorPanel.parentElement.getBoundingClientRect();
      var pct = ((e.clientX - mainRect.left) / mainRect.width) * 100;
      pct = Math.max(20, Math.min(80, pct));
      editorPanel.style.flex = '0 0 ' + pct + '%';
      if (editor) editor.layout();
    });

    document.addEventListener('mouseup', function () {
      if (!dragging) return;
      dragging = false;
      resizeHandle.classList.remove('active');
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    });
  })();

  // ── Monaco editor ──

  function initMonaco() {
    require.config({
      paths: { vs: 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs' }
    });

    require(['vs/editor/editor.main'], function () {
      editor = monaco.editor.create(editorContainer, {
        value: EXAMPLES.hello,
        language: 'c',
        theme: 'vs-dark',
        fontSize: 14,
        fontFamily: "'JetBrains Mono', 'Cascadia Code', monospace",
        minimap: { enabled: false },
        automaticLayout: true,
        scrollBeyondLastLine: false,
        lineNumbers: 'on',
        renderWhitespace: 'none',
        tabSize: 4,
        padding: { top: 12, bottom: 12 }
      });

      editor.addAction({
        id: 'compile-run',
        label: 'Compile & Run',
        keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter],
        run: function () { compileAndRun(); }
      });

      setStatus('Ready — Ctrl+Enter to compile');
    });
  }

  // ── wabt.js (lazy-loaded) ──

  function loadWabt() {
    if (wabtInstance) return Promise.resolve(wabtInstance);

    return new Promise(function (resolve, reject) {
      // Temporarily hide AMD define so wabt registers as a global, not AMD
      var origDefine = window.define;
      window.define = undefined;

      var script = document.createElement('script');
      script.src = 'https://cdn.jsdelivr.net/npm/wabt@1.0.36/index.js';
      script.onload = function () {
        window.define = origDefine;
        var factory = window.WabtModule || window.wabt;
        if (typeof factory !== 'function') {
          reject(new Error('wabt.js loaded but factory function not found'));
          return;
        }
        factory().then(function (w) {
          wabtInstance = w;
          resolve(w);
        }).catch(reject);
      };
      script.onerror = function () {
        window.define = origDefine;
        reject(new Error('Failed to load wabt.js from CDN'));
      };
      document.head.appendChild(script);
    });
  }

  // ── WASI shim for running compiled programs ──

  function runWasm(wasmBytes) {
    var output = '';
    var memory = null;

    var importObject = {
      wasi_snapshot_preview1: {
        proc_exit: function (code) {
          throw { type: 'proc_exit', code: code };
        },
        fd_write: function (fd, iovs, iovs_len, nwritten_ptr) {
          var view = new DataView(memory.buffer);
          var bytes = new Uint8Array(memory.buffer);
          var totalWritten = 0;
          for (var i = 0; i < iovs_len; i++) {
            var ptr = view.getUint32(iovs + i * 8, true);
            var len = view.getUint32(iovs + i * 8 + 4, true);
            var chunk = bytes.slice(ptr, ptr + len);
            output += new TextDecoder().decode(chunk);
            totalWritten += len;
          }
          view.setUint32(nwritten_ptr, totalWritten, true);
          return 0;
        },
        fd_read: function (fd, iovs, iovs_len, nread_ptr) {
          var view = new DataView(memory.buffer);
          view.setUint32(nread_ptr, 0, true); // EOF
          return 0;
        }
      }
    };

    var module = new WebAssembly.Module(wasmBytes);
    var instance = new WebAssembly.Instance(module, importObject);
    memory = instance.exports.memory;

    try {
      instance.exports._start();
    } catch (e) {
      if (e && e.type === 'proc_exit') {
        if (e.code !== 0) {
          output += '\n[Process exited with code ' + e.code + ']';
        }
      } else {
        throw e;
      }
    }

    return output;
  }

  // ── Error parsing + Monaco markers ──

  function parseErrors(text) {
    var errors = [];
    var lines = text.split('\n');
    for (var i = 0; i < lines.length; i++) {
      var m = lines[i].match(/line\s+(\d+)(?:[,:]\s*col(?:umn)?\s+(\d+))?[:\s]+(.*)/i);
      if (m) {
        errors.push({
          line: parseInt(m[1], 10),
          col: m[2] ? parseInt(m[2], 10) : 1,
          message: m[3].trim()
        });
      }
    }
    return errors;
  }

  function setErrorMarkers(errors) {
    if (!editor) return;
    var markers = errors.map(function (err) {
      return {
        startLineNumber: err.line,
        startColumn: err.col,
        endLineNumber: err.line,
        endColumn: err.col + 100,
        message: err.message,
        severity: monaco.MarkerSeverity.Error
      };
    });
    monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', markers);
  }

  // ── UI helpers ──

  function setStatus(text) {
    statusEl.textContent = text;
  }

  function setButtonState(state) {
    if (idleTimerId) {
      clearTimeout(idleTimerId);
      idleTimerId = null;
    }
    runBtn.className = '';
    switch (state) {
      case 'compiling':
        runBtn.textContent = '⏳ Compiling…';
        runBtn.disabled = true;
        break;
      case 'assembling':
        runBtn.textContent = '⏳ Assembling…';
        runBtn.disabled = true;
        break;
      case 'running':
        runBtn.textContent = '⏳ Running…';
        runBtn.disabled = true;
        break;
      case 'success':
        runBtn.textContent = '✓ Done';
        runBtn.disabled = false;
        runBtn.className = 'success';
        idleTimerId = setTimeout(function () { setButtonState('idle'); }, 2000);
        break;
      case 'error':
        runBtn.textContent = '✗ Error';
        runBtn.disabled = false;
        runBtn.className = 'error';
        idleTimerId = setTimeout(function () { setButtonState('idle'); }, 2000);
        break;
      default:
        runBtn.textContent = '▶ Compile & Run';
        runBtn.disabled = false;
    }
  }

  function writeConsole(text, className) {
    var span = document.createElement('span');
    if (className) span.className = className;
    span.textContent = text;
    consoleOutput.appendChild(span);
  }

  function clearOutput() {
    consoleOutput.textContent = '';
    watOutput.textContent = '';
  }

  // ── Compile & Run pipeline ──

  function compileAndRun() {
    if (!editor || isRunning) return;
    isRunning = true;

    var source = editor.getValue();
    clearOutput();
    monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
    activateTab('console');

    // Step 1: C → WAT
    setButtonState('compiling');
    setStatus('Compiling C → WAT…');

    CompilerAPI.compile(source)
      .then(function (watText) {
        watOutput.textContent = watText;

        // Step 2: WAT → WASM
        setButtonState('assembling');
        setStatus('Assembling WAT → WASM…');

        return loadWabt().then(function (wabt) {
          var wabtModule = wabt.parseWat('output.wat', watText);
          var result = wabtModule.toBinary({ log: false, write_debug_names: false });
          wabtModule.destroy();
          return result.buffer;
        });
      })
      .then(function (wasmBytes) {
        // Step 3: Run
        setButtonState('running');
        setStatus('Running…');

        var output = runWasm(wasmBytes);
        writeConsole(output || '(no output)', output ? '' : 'output-info');
        setButtonState('success');
        setStatus('Done');
      })
      .catch(function (err) {
        var msg = err.message || String(err);

        // Try to parse compiler errors for Monaco markers
        var errors = parseErrors(msg);
        if (errors.length > 0) {
          setErrorMarkers(errors);
        }

        writeConsole(msg, 'output-error');
        setButtonState('error');
        setStatus('Error');
      })
      .then(function () { isRunning = false; }, function () { isRunning = false; });
  }

  // ── Bind events ──

  runBtn.addEventListener('click', compileAndRun);

  // ── Boot ──

  setStatus('Loading editor…');
  initMonaco();
})();
