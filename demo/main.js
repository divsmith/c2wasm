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
      '}\n',

    collatz:
      '// Collatz sequence (3n+1 problem)\n' +
      '// Features: do-while, ternary (?:), post-increment (count++)\n' +
      '\n' +
      'int steps(int n) {\n' +
      '    int count = 0;\n' +
      '    do {\n' +
      '        n = (n % 2 == 0) ? n / 2 : n * 3 + 1;\n' +
      '        count++;\n' +
      '    } while (n != 1);\n' +
      '    return count;\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    int n;\n' +
      '    int s;\n' +
      '    int max_steps = 0;\n' +
      '    int max_n = 0;\n' +
      '    for (n = 1; n <= 20; n++) {\n' +
      '        s = steps(n);\n' +
      '        printf("collatz(%d) = %d steps\\n", n, s);\n' +
      '        if (s > max_steps) {\n' +
      '            max_steps = s;\n' +
      '            max_n = n;\n' +
      '        }\n' +
      '    }\n' +
      '    printf("Longest: %d with %d steps\\n", max_n, max_steps);\n' +
      '    return 0;\n' +
      '}\n',

    weekday:
      '// Day of Week\n' +
      '// Features: enum, typedef, const, switch/case, ternary\n' +
      '\n' +
      'enum Weekday { MON = 1, TUE, WED, THU, FRI, SAT, SUN };\n' +
      'typedef int Day;\n' +
      '\n' +
      'const int WORK_DAYS = 5;\n' +
      '\n' +
      'int is_weekend(Day d) {\n' +
      '    return (d == SAT || d == SUN) ? 1 : 0;\n' +
      '}\n' +
      '\n' +
      'void print_day(Day d) {\n' +
      '    switch (d) {\n' +
      '        case MON: printf("Monday   "); break;\n' +
      '        case TUE: printf("Tuesday  "); break;\n' +
      '        case WED: printf("Wednesday"); break;\n' +
      '        case THU: printf("Thursday "); break;\n' +
      '        case FRI: printf("Friday   "); break;\n' +
      '        case SAT: printf("Saturday "); break;\n' +
      '        case SUN: printf("Sunday   "); break;\n' +
      '        default:  printf("Unknown  "); break;\n' +
      '    }\n' +
      '    if (is_weekend(d)) {\n' +
      '        printf(" (weekend)\\n");\n' +
      '    } else {\n' +
      '        printf(" (weekday)\\n");\n' +
      '    }\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    Day d;\n' +
      '    int weekends = 0;\n' +
      '    for (d = MON; d <= SUN; d++) {\n' +
      '        weekends += is_weekend(d);\n' +
      '        print_day(d);\n' +
      '    }\n' +
      '    printf("%d work days, %d weekend days\\n", WORK_DAYS, weekends);\n' +
      '    return 0;\n' +
      '}\n',

    bit_flags:
      '// File permissions with bit flags\n' +
      '// Features: typedef, const, compound bitwise ops (|=, &=, ^=), ~, ternary\n' +
      '\n' +
      'typedef int Perms;\n' +
      '\n' +
      'const int READ  = 4;\n' +
      'const int WRITE = 2;\n' +
      'const int EXEC  = 1;\n' +
      '\n' +
      'void show(Perms p) {\n' +
      '    int r;\n' +
      '    int w;\n' +
      '    int x;\n' +
      '    r = (p & READ)  ? 1 : 0;\n' +
      '    w = (p & WRITE) ? 1 : 0;\n' +
      '    x = (p & EXEC)  ? 1 : 0;\n' +
      '    printf("r=%d w=%d x=%d\\n", r, w, x);\n' +
      '}\n' +
      '\n' +
      'int main() {\n' +
      '    Perms owner;\n' +
      '    Perms group;\n' +
      '    Perms other;\n' +
      '\n' +
      '    owner = READ | WRITE | EXEC;   /* rwx = 7 */\n' +
      '    group = READ | EXEC;           /* r-x = 5 */\n' +
      '    other = READ;                  /* r-- = 4 */\n' +
      '\n' +
      '    printf("Initial:\\n");\n' +
      '    printf("  owner: "); show(owner);\n' +
      '    printf("  group: "); show(group);\n' +
      '    printf("  other: "); show(other);\n' +
      '\n' +
      '    owner ^= WRITE;       /* revoke write from owner */\n' +
      '    group |= WRITE;       /* grant write to group */\n' +
      '    other &= ~EXEC;       /* ensure no exec for other */\n' +
      '\n' +
      '    printf("After chmod:\\n");\n' +
      '    printf("  owner: "); show(owner);\n' +
      '    printf("  group: "); show(group);\n' +
      '    printf("  other: "); show(other);\n' +
      '    return 0;\n' +
      '}\n',

    greet:
      '// Reads a name from stdin and prints a greeting.\n' +
      '// Type your name in the terminal below, then press Enter!\n' +
      'int main() {\n' +
      '    char name[64];\n' +
      '    int i;\n' +
      '    int c;\n' +
      '    printf("What is your name? ");\n' +
      '    i = 0;\n' +
      '    while (i < 63) {\n' +
      '        c = getchar();\n' +
      '        if (c == -1 || c == 10) {\n' +
      '            break;\n' +
      '        }\n' +
      '        name[i] = c;\n' +
      '        i = i + 1;\n' +
      '    }\n' +
      '    name[i] = 0;\n' +
      '    printf("Hello, %s!\\n", name);\n' +
      '    return 0;\n' +
      '}\n'
  };

  var STORAGE_KEYS = {
    FILES: 'c2wasm:files',
    ACTIVE: 'c2wasm:active'
  };

  function loadUserFiles() {
    try {
      var raw = localStorage.getItem(STORAGE_KEYS.FILES);
      return raw ? JSON.parse(raw) : [];
    } catch (e) { return []; }
  }

  function saveUserFiles(files) {
    try {
      localStorage.setItem(STORAGE_KEYS.FILES, JSON.stringify(files));
      return true;
    } catch (e) {
      console.warn('c2wasm: localStorage write failed', e);
      return false;
    }
  }

  function getActiveFile() {
    try {
      var raw = localStorage.getItem(STORAGE_KEYS.ACTIVE);
      return raw ? JSON.parse(raw) : { type: 'example', id: 'hello' };
    } catch (e) { return { type: 'example', id: 'hello' }; }
  }

  function setActiveFile(type, id) {
    try {
      localStorage.setItem(STORAGE_KEYS.ACTIVE, JSON.stringify({ type: type, id: id }));
    } catch (e) {}
  }

  function generateId() {
    return Date.now().toString(36) + Math.random().toString(36).substr(2, 6);
  }

  function getFileContent(type, id) {
    if (type === 'example') {
      return EXAMPLES[id] || EXAMPLES.hello;
    }
    var files = loadUserFiles();
    for (var i = 0; i < files.length; i++) {
      if (files[i].id === id) return files[i].content;
    }
    return null; // null = file not found; distinguish from empty string (valid empty file)
  }

  // ── DOM references ──

  var editorContainer = document.getElementById('editor-container');
  var fileSelect = document.getElementById('file-select');
  var userFilesGroup = document.getElementById('user-files-group');
  var newFileBtn = document.getElementById('new-file-btn');
  var deleteFileBtn = document.getElementById('delete-file-btn');
  var runBtn = document.getElementById('run-btn');
  var statusEl = document.getElementById('status');
  var saveIndicator = document.getElementById('save-indicator');
  var consoleTabContent = document.getElementById('console-tab-content');
  var consoleOutput = document.getElementById('console-output');
  var terminalInputRow = document.getElementById('terminal-input-row');
  var terminalInput = document.getElementById('terminal-input');
  var watOutput = document.getElementById('wat-output');
  var tabs = document.querySelectorAll('.tab');
  var resizeHandle = document.getElementById('resize-handle');

  var editor = null;
  var wabtInstance = null;
  var isRunning = false;
  var idleTimerId = null;
  var autoSaveTimer = null;

  // ── File selector helpers ──

  function refreshFileSelector() {
    var files = loadUserFiles();
    userFilesGroup.innerHTML = '';
    for (var i = 0; i < files.length; i++) {
      var opt = document.createElement('option');
      opt.value = 'user:' + files[i].id;
      opt.textContent = files[i].name;
      userFilesGroup.appendChild(opt);
    }
    // optgroup is always in DOM; toggling disabled hides it in most browsers
    userFilesGroup.disabled = files.length === 0;
  }

  function syncSelectToActive() {
    var active = getActiveFile();
    fileSelect.value = active.type === 'user' ? 'user:' + active.id : active.id;
    deleteFileBtn.style.display = active.type === 'user' ? '' : 'none';
  }

  function showSaveIndicator() {
    saveIndicator.textContent = '● Saved';
    saveIndicator.className = 'save-indicator saved';
    setTimeout(function () {
      saveIndicator.textContent = '';
      saveIndicator.className = 'save-indicator';
    }, 1500);
  }

  function showSaveError() {
    saveIndicator.textContent = '⚠ Save failed';
    saveIndicator.className = 'save-indicator save-error';
    setTimeout(function () {
      saveIndicator.textContent = '';
      saveIndicator.className = 'save-indicator';
    }, 3000);
  }

  // Flush any pending debounced auto-save synchronously before switching files.
  // Guards against losing edits when the user switches files within 800ms of typing.
  function flushPendingSave() {
    if (!autoSaveTimer) return;
    clearTimeout(autoSaveTimer);
    autoSaveTimer = null;
    var active = getActiveFile();
    if (active.type !== 'user' || !editor) return;
    var files = loadUserFiles();
    for (var i = 0; i < files.length; i++) {
      if (files[i].id === active.id) {
        files[i].content = editor.getValue();
        files[i].modified = Date.now();
        saveUserFiles(files); // best-effort; errors already warned to console
        break;
      }
    }
  }

  function scheduleAutoSave() {
    if (autoSaveTimer) clearTimeout(autoSaveTimer);
    autoSaveTimer = setTimeout(function () {
      autoSaveTimer = null;
      var active = getActiveFile();
      if (active.type !== 'user' || !editor) return;
      var files = loadUserFiles();
      for (var i = 0; i < files.length; i++) {
        if (files[i].id === active.id) {
          files[i].content = editor.getValue();
          files[i].modified = Date.now();
          if (saveUserFiles(files)) {
            showSaveIndicator();
          } else {
            showSaveError();
          }
          break;
        }
      }
    }, 800);
  }

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
    consoleTabContent.classList.toggle('hidden', name !== 'console');
    watOutput.classList.toggle('hidden', name !== 'wat');
  }

  for (var i = 0; i < tabs.length; i++) {
    (function (tab) {
      tab.addEventListener('click', function () {
        activateTab(tab.dataset.tab);
      });
    })(tabs[i]);
  }

  // ── File selector events ──

  fileSelect.addEventListener('change', function () {
    flushPendingSave();
    var val = fileSelect.value;
    var type, id;
    if (val.indexOf('user:') === 0) {
      type = 'user';
      id = val.slice(5);
    } else {
      type = 'example';
      id = val;
    }
    setActiveFile(type, id);
    deleteFileBtn.style.display = type === 'user' ? '' : 'none';
    var code = getFileContent(type, id);
    if (code === null) {
      // User file was deleted from another tab or storage; fall back gracefully
      setActiveFile('example', 'hello');
      fileSelect.value = 'hello';
      deleteFileBtn.style.display = 'none';
      code = EXAMPLES.hello;
    }
    if (editor) {
      editor.setValue(code);
      monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
    }
  });

  newFileBtn.addEventListener('click', function () {
    flushPendingSave();
    var name = prompt('File name:', 'program.c');
    if (!name) return;
    name = name.trim();
    if (!name) return;
    if (name.indexOf('.') === -1) name += '.c';

    var id = generateId();
    var files = loadUserFiles();
    var newFile = {
      id: id,
      name: name,
      content: '// ' + name + '\n\nint main() {\n    return 0;\n}\n',
      created: Date.now(),
      modified: Date.now()
    };
    files.push(newFile);
    saveUserFiles(files);

    refreshFileSelector();
    fileSelect.value = 'user:' + id;
    setActiveFile('user', id);
    deleteFileBtn.style.display = '';

    if (editor) {
      editor.setValue(newFile.content);
      monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
      editor.focus();
    }
  });

  deleteFileBtn.addEventListener('click', function () {
    flushPendingSave();
    var active = getActiveFile();
    if (active.type !== 'user') return;
    var files = loadUserFiles();
    var file = null;
    for (var i = 0; i < files.length; i++) {
      if (files[i].id === active.id) { file = files[i]; break; }
    }
    if (!confirm('Delete "' + (file ? file.name : 'this file') + '"? This cannot be undone.')) return;
    saveUserFiles(files.filter(function (f) { return f.id !== active.id; }));

    refreshFileSelector();
    fileSelect.value = 'hello';
    setActiveFile('example', 'hello');
    deleteFileBtn.style.display = 'none';

    if (editor) {
      editor.setValue(EXAMPLES.hello);
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
      // Restore last active file from localStorage
      var active = getActiveFile();
      var initialContent;
      if (active.type === 'user') {
        initialContent = getFileContent('user', active.id);
        if (initialContent === null) {
          // File was deleted; fall back to hello example
          active = { type: 'example', id: 'hello' };
          setActiveFile('example', 'hello');
          initialContent = EXAMPLES.hello;
        }
      } else {
        initialContent = EXAMPLES[active.id] || EXAMPLES.hello;
      }

      editor = monaco.editor.create(editorContainer, {
        value: initialContent,
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

      syncSelectToActive();

      editor.onDidChangeModelContent(function () {
        scheduleAutoSave();
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

  // ── Interactive WASM runner (Web Worker + SharedArrayBuffer) ──

  // SAB layout: Int32[0]=signal, Int32[1]=byte count, bytes from offset 8.
  var SAB_SIZE = 8 + 4096;

  var activeWorker = null;

  function showTerminalInput(sab, sabInt32) {
    terminalInputRow.classList.remove('hidden');
    terminalInput.value = '';
    terminalInput.focus();
    // Scroll so the input line is visible
    consoleOutput.scrollTop = consoleOutput.scrollHeight;

    function onEnter(e) {
      if (e.key !== 'Enter') return;
      terminalInput.removeEventListener('keydown', onEnter);

      var text = terminalInput.value + '\n';
      hideTerminalInput();

      // Echo what the user typed into the terminal output
      writeConsole(text);

      // Write input to SAB and wake the worker
      var encoded = new TextEncoder().encode(text);
      var toWrite = Math.min(encoded.length, SAB_SIZE - 8);
      new Uint8Array(sab, 8, toWrite).set(encoded.subarray(0, toWrite));
      Atomics.store(sabInt32, 1, toWrite);
      Atomics.store(sabInt32, 0, 1);
      Atomics.notify(sabInt32, 0, 1);
    }

    terminalInput.addEventListener('keydown', onEnter);
  }

  function hideTerminalInput() {
    terminalInputRow.classList.add('hidden');
    terminalInput.value = '';
  }

  function runWasmInteractive(wasmBytes) {
    if (typeof SharedArrayBuffer === 'undefined') {
      return Promise.reject(new Error(
        'SharedArrayBuffer is not available in this context.\n' +
        'Run locally with: cd compiler && make serve\n' +
        '(The dev server sets the required cross-origin isolation headers.)'
      ));
    }

    return new Promise(function (resolve, reject) {
      var sab = new SharedArrayBuffer(SAB_SIZE);
      var sabInt32 = new Int32Array(sab);
      var hasOutput = false;

      var worker = new Worker('./wasm-worker.js');
      activeWorker = worker;

      worker.onmessage = function (event) {
        var msg = event.data;
        if (msg.type === 'output') {
          hasOutput = true;
          writeConsole(msg.text);
        } else if (msg.type === 'needInput') {
          showTerminalInput(sab, sabInt32);
        } else if (msg.type === 'exit') {
          activeWorker = null;
          worker.terminate();
          hideTerminalInput();
          if (!hasOutput) writeConsole('(no output)', 'output-info');
          if (msg.code !== 0) {
            writeConsole('\n[Process exited with code ' + msg.code + ']', 'output-info');
          }
          resolve(msg.code);
        } else if (msg.type === 'error') {
          activeWorker = null;
          worker.terminate();
          hideTerminalInput();
          reject(new Error(msg.message));
        }
      };

      worker.onerror = function (e) {
        activeWorker = null;
        worker.terminate();
        hideTerminalInput();
        reject(new Error(e.message || 'Worker error'));
      };

      // Transfer wasmBytes to the worker (zero-copy)
      worker.postMessage({ type: 'run', wasmBytes: wasmBytes, sab: sab }, [wasmBytes]);
    });
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
    // Auto-scroll if already near the bottom
    var nearBottom = consoleOutput.scrollHeight - consoleOutput.scrollTop - consoleOutput.clientHeight < 60;
    if (nearBottom) consoleOutput.scrollTop = consoleOutput.scrollHeight;
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

    setButtonState('compiling');
    setStatus('Compiling…');

    CompilerAPI.compileBinary(source)
      .then(function (bytes) {
        /* Check if output is WASM binary (starts with \0asm magic) */
        if (bytes.length >= 4 && bytes[0] === 0x00 && bytes[1] === 0x61
            && bytes[2] === 0x73 && bytes[3] === 0x6D) {
          /* Binary mode — skip wabt entirely */
          watOutput.textContent = '(Binary output mode — WAT not available)';
          return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
        }

        /* WAT mode — decode as text and assemble via wabt */
        var watText = new TextDecoder().decode(bytes);
        watOutput.textContent = watText;

        setButtonState('assembling');
        setStatus('Assembling WAT → WASM…');

        return loadWabt().then(function (wabt) {
          var wabtModule = wabt.parseWat('output.wat', watText);
          var result = wabtModule.toBinary({ log: false, write_debug_names: false });
          wabtModule.destroy();
          var typed = result.buffer;
          return typed.buffer.slice(typed.byteOffset, typed.byteOffset + typed.byteLength);
        });
      })
      .then(function (wasmBytes) {
        setButtonState('running');
        setStatus('Running…');

        return runWasmInteractive(wasmBytes).then(function (exitCode) {
          setButtonState(exitCode === 0 ? 'success' : 'error');
          setStatus(exitCode === 0 ? 'Done' : 'Exited with code ' + exitCode);
        });
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

  // ── Service worker (cross-origin isolation for SharedArrayBuffer) ──

  if (!self.crossOriginIsolated && 'serviceWorker' in navigator) {
    // Register the COI service worker and reload once so SharedArrayBuffer
    // becomes available (needed for interactive stdin via Atomics.wait).
    var reloadKey = 'coiReloaded';
    if (!sessionStorage.getItem(reloadKey)) {
      sessionStorage.setItem(reloadKey, '1');
      navigator.serviceWorker.register('./coi-serviceworker.js').then(function () {
        window.location.reload();
      });
    }
  }

  // ── Boot ──

  setStatus('Loading editor…');
  refreshFileSelector();
  initMonaco();
})();
