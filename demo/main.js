'use strict';

(function () {
  // ── Example programs (fetched from demo/examples/*.c) ──

  var exampleCache = {};

  function fetchExample(id, cb) {
    if (exampleCache[id] !== undefined) { cb(exampleCache[id]); return; }
    fetch('examples/' + id + '.c')
      .then(function (r) { return r.ok ? r.text() : Promise.reject(r.status); })
      .then(function (text) { exampleCache[id] = text; cb(text); })
      .catch(function () { cb(exampleCache['hello'] || '// Error: could not load example (requires HTTP server)\n'); });
  }

  var STORAGE_KEYS = {
    FILES: 'c2wasm:files',
    ACTIVE: 'c2wasm:active',
    COMPILER_EDITS: 'c2wasm:compiler-edits',
    ACTIVE_COMPILER_FILE: 'c2wasm:compiler-active-file'
  };

  // ── Compiler source working copy ──

  var compilerWorkingCopy = {};
  var currentEditorMode = 'program'; // 'program' or 'compiler'

  function initCompilerWorkingCopy() {
    // Load saved edits from localStorage, or start fresh from bundled source
    var saved = null;
    try {
      var raw = localStorage.getItem(STORAGE_KEYS.COMPILER_EDITS);
      if (raw) saved = JSON.parse(raw);
    } catch (e) {}

    var keys = Object.keys(COMPILER_SOURCE);
    for (var i = 0; i < keys.length; i++) {
      compilerWorkingCopy[keys[i]] = (saved && saved[keys[i]] !== undefined)
        ? saved[keys[i]]
        : COMPILER_SOURCE[keys[i]];
    }
  }

  function saveCompilerEdits() {
    // Only save files that differ from the bundled source
    var diff = {};
    var keys = Object.keys(compilerWorkingCopy);
    var hasChanges = false;
    for (var i = 0; i < keys.length; i++) {
      if (compilerWorkingCopy[keys[i]] !== COMPILER_SOURCE[keys[i]]) {
        diff[keys[i]] = compilerWorkingCopy[keys[i]];
        hasChanges = true;
      }
    }
    try {
      if (hasChanges) {
        localStorage.setItem(STORAGE_KEYS.COMPILER_EDITS, JSON.stringify(diff));
      } else {
        localStorage.removeItem(STORAGE_KEYS.COMPILER_EDITS);
      }
    } catch (e) {}
  }

  function hasCompilerChanges() {
    var keys = Object.keys(COMPILER_SOURCE);
    for (var i = 0; i < keys.length; i++) {
      if (compilerWorkingCopy[keys[i]] !== COMPILER_SOURCE[keys[i]]) return true;
    }
    return false;
  }

  function getActiveCompilerFile() {
    try {
      return localStorage.getItem(STORAGE_KEYS.ACTIVE_COMPILER_FILE) || 'c2wasm.c';
    } catch (e) { return 'c2wasm.c'; }
  }

  function setActiveCompilerFile(name) {
    try { localStorage.setItem(STORAGE_KEYS.ACTIVE_COMPILER_FILE, name); } catch (e) {}
  }

  initCompilerWorkingCopy();

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
      // Return from cache; may be undefined if not yet fetched.
      return exampleCache[id] !== undefined ? exampleCache[id] : null;
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
  var resizeHandle = document.getElementById('resize-handle');

  // Compiler mode DOM
  var modeProgramBtn = document.getElementById('mode-program');
  var modeCompilerBtn = document.getElementById('mode-compiler');
  var programControls = document.getElementById('program-controls');
  var compilerControls = document.getElementById('compiler-controls');
  var compilerFileSelect = document.getElementById('compiler-file-select');
  var buildCompilerBtn = document.getElementById('build-compiler-btn');
  var resetSourceBtn = document.getElementById('reset-source-btn');
  var compilerModeIndicator = document.getElementById('compiler-mode-indicator');

  // Debug UI DOM
  var debugBtn = document.getElementById('debug-btn');
  var debugControls = document.getElementById('debug-controls');
  var debugStepIntoBtn = document.getElementById('debug-step-into-btn');
  var debugStepOverBtn = document.getElementById('debug-step-over-btn');
  var debugContinueBtn = document.getElementById('debug-continue-btn');
  var debugStopBtn = document.getElementById('debug-stop-btn');
  var debugStatusEl = document.getElementById('debug-status');

  var editor = null;
  var isRunning = false;
  var idleTimerId = null;
  var autoSaveTimer = null;

  // ── Debugger state ──

  // SAB slot index for debug pause signal (Int32Array)
  var DBG_PAUSE_SLOT = 2;

  var debugWorker            = null;
  var debugSabInt32          = null;
  var debugCurrentDecorations = [];
  var breakpointDecorations  = [];
  var debugBreakpoints       = new Set(); // line numbers (1-based) with active breakpoints
  // 'run'       = execute normally, only pause on breakpoints
  // 'step'      = pause after the very next traced statement (Step Into)
  // 'step-over' = pause at the next statement at the same or shallower call depth
  var debugMode      = 'run';
  var currentDepth   = 0; // call depth from the most recent trace message
  var stepOverDepth  = 0; // target depth for step-over

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
      if (currentEditorMode === 'compiler') {
        // Auto-save compiler source edits
        if (!editor) return;
        var fname = getActiveCompilerFile();
        compilerWorkingCopy[fname] = editor.getValue();
        saveCompilerEdits();
        showSaveIndicator();
        return;
      }
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

    if (type === 'example') {
      fetchExample(id, function (code) {
        if (editor) {
          clearBreakpoints();
          editor.setValue(code);
          monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
        }
      });
      return;
    }

    var code = getFileContent(type, id);
    if (code === null) {
      // User file was deleted from another tab or storage; fall back gracefully
      setActiveFile('example', 'hello');
      fileSelect.value = 'hello';
      deleteFileBtn.style.display = 'none';
      code = exampleCache['hello'] || '';
    }
    if (editor) {
      clearBreakpoints();
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
      clearBreakpoints();
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
      clearBreakpoints();
      editor.setValue(exampleCache['hello'] || '');
      monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
    }
  });

  // ── Panel resize ──

  (function () {
    var dragging = false;
    var editorPanel = document.querySelector('.editor-panel');
    var mobileQuery = window.matchMedia('(max-width: 768px)');

    function applyResize(clientX, clientY) {
      var mainRect = editorPanel.parentElement.getBoundingClientRect();
      var pct;
      if (mobileQuery.matches) {
        pct = ((clientY - mainRect.top) / mainRect.height) * 100;
      } else {
        pct = ((clientX - mainRect.left) / mainRect.width) * 100;
      }
      pct = Math.max(20, Math.min(80, pct));
      editorPanel.style.flex = '0 0 ' + pct + '%';
      if (editor) editor.layout();
    }

    // Mouse resize
    resizeHandle.addEventListener('mousedown', function (e) {
      e.preventDefault();
      dragging = true;
      resizeHandle.classList.add('active');
      document.body.style.cursor = mobileQuery.matches ? 'row-resize' : 'col-resize';
      document.body.style.userSelect = 'none';
    });

    document.addEventListener('mousemove', function (e) {
      if (!dragging) return;
      applyResize(e.clientX, e.clientY);
    });

    document.addEventListener('mouseup', function () {
      if (!dragging) return;
      dragging = false;
      resizeHandle.classList.remove('active');
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    });

    // Touch resize
    resizeHandle.addEventListener('touchstart', function (e) {
      e.preventDefault();
      dragging = true;
      resizeHandle.classList.add('active');
      document.body.style.userSelect = 'none';
    }, { passive: false });

    document.addEventListener('touchmove', function (e) {
      if (!dragging) return;
      e.preventDefault();
      var touch = e.touches[0];
      applyResize(touch.clientX, touch.clientY);
    }, { passive: false });

    document.addEventListener('touchend', function () {
      if (!dragging) return;
      dragging = false;
      resizeHandle.classList.remove('active');
      document.body.style.userSelect = '';
    });

    // On breakpoint crossing (e.g. device rotation), clear any inline resize
    // so the CSS default kicks back in
    mobileQuery.addEventListener('change', function () {
      editorPanel.style.flex = '';
      if (editor) editor.layout();
    });
  })();

  // ── Mode switching (Program ↔ Compiler) ──

  function populateCompilerFileSelect() {
    compilerFileSelect.innerHTML = '';
    for (var i = 0; i < COMPILER_SOURCE_ORDER.length; i++) {
      var opt = document.createElement('option');
      opt.value = COMPILER_SOURCE_ORDER[i];
      opt.textContent = COMPILER_SOURCE_ORDER[i];
      compilerFileSelect.appendChild(opt);
    }
    compilerFileSelect.value = getActiveCompilerFile();
  }

  function saveCurrentEditorContent() {
    if (!editor) return;
    if (currentEditorMode === 'program') {
      flushPendingSave();
    } else {
      var fname = getActiveCompilerFile();
      compilerWorkingCopy[fname] = editor.getValue();
      saveCompilerEdits();
    }
  }

  function switchMode(mode) {
    if (mode === currentEditorMode) return;
    saveCurrentEditorContent();
    currentEditorMode = mode;

    modeProgramBtn.classList.toggle('active', mode === 'program');
    modeCompilerBtn.classList.toggle('active', mode === 'compiler');

    if (mode === 'program') {
      programControls.style.display = '';
      compilerControls.classList.add('hidden');
      if (editor) editor.updateOptions({ glyphMargin: true });
      // Restore program file
      var active = getActiveFile();
      if (active.type === 'example') {
        fetchExample(active.id, function (code) {
          if (editor) {
            clearBreakpoints();
            editor.setValue(code);
            monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
          }
        });
      } else {
        var code = getFileContent(active.type, active.id);
        if (code === null) {
          setActiveFile('example', 'hello');
          fileSelect.value = 'hello';
          code = exampleCache['hello'] || '';
        }
        if (editor) {
          clearBreakpoints();
          editor.setValue(code);
          monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
        }
      }
      syncSelectToActive();
    } else {
      programControls.style.display = 'none';
      compilerControls.classList.remove('hidden');
      if (editor) editor.updateOptions({ glyphMargin: false });
      var fname = getActiveCompilerFile();
      if (editor) {
        editor.setValue(compilerWorkingCopy[fname] || '');
        monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
      }
    }
  }

  function updateCompilerModeIndicator() {
    var mode = CompilerAPI.getMode();
    var hasCustom = CompilerAPI.hasCustomCompiler();
    if (hasCustom && mode === 'custom') {
      compilerModeIndicator.textContent = 'Using: Custom ✓';
      compilerModeIndicator.className = 'compiler-mode-indicator custom';
    } else {
      compilerModeIndicator.textContent = 'Using: Reference';
      compilerModeIndicator.className = 'compiler-mode-indicator';
    }
  }

  modeProgramBtn.addEventListener('click', function () { switchMode('program'); });
  modeCompilerBtn.addEventListener('click', function () { switchMode('compiler'); });

  compilerFileSelect.addEventListener('change', function () {
    if (currentEditorMode !== 'compiler' || !editor) return;
    // Save current file before switching
    var prev = getActiveCompilerFile();
    compilerWorkingCopy[prev] = editor.getValue();
    saveCompilerEdits();
    // Load new file
    var fname = compilerFileSelect.value;
    setActiveCompilerFile(fname);
    editor.setValue(compilerWorkingCopy[fname] || '');
    monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
  });

  // ── Build Compiler ──

  var isBuilding = false;

  function setBuildButtonState(state) {
    buildCompilerBtn.className = '';
    switch (state) {
      case 'building':
        buildCompilerBtn.textContent = '⏳ Building…';
        buildCompilerBtn.disabled = true;
        break;
      case 'success':
        buildCompilerBtn.textContent = '✓ Built';
        buildCompilerBtn.disabled = false;
        buildCompilerBtn.className = 'success';
        setTimeout(function () { setBuildButtonState('idle'); }, 2000);
        break;
      case 'error':
        buildCompilerBtn.textContent = '✗ Failed';
        buildCompilerBtn.disabled = false;
        buildCompilerBtn.className = 'error';
        setTimeout(function () { setBuildButtonState('idle'); }, 2000);
        break;
      default:
        buildCompilerBtn.textContent = '🔨 Build Compiler';
        buildCompilerBtn.disabled = false;
    }
  }

  function buildCompiler() {
    if (isBuilding) return;
    isBuilding = true;
    saveCurrentEditorContent();

    clearOutput();
    setBuildButtonState('building');
    setStatus('Building custom compiler…');

    var t0 = Date.now();

    CompilerAPI.buildCompiler(compilerWorkingCopy)
      .then(function (result) {
        var elapsed = ((Date.now() - t0) / 1000).toFixed(1);
        var sizeKB = (result.size / 1024).toFixed(0);
        writeConsole('Custom compiler built in ' + elapsed + 's (' + sizeKB + ' KB)\n', 'output-success');
        setBuildButtonState('success');
        setStatus('Custom compiler active');
        updateCompilerModeIndicator();
      })
      .catch(function (err) {
        var msg = err.message || String(err);
        var errors = parseErrors(msg);
        if (errors.length > 0) setErrorMarkers(errors);
        writeConsole('Build failed:\n' + msg, 'output-error');
        setBuildButtonState('error');
        setStatus('Build failed');
      })
      .then(function () { isBuilding = false; }, function () { isBuilding = false; });
  }

  buildCompilerBtn.addEventListener('click', buildCompiler);

  resetSourceBtn.addEventListener('click', function () {
    if (!hasCompilerChanges()) return;
    if (!confirm('Reset all compiler source changes? This cannot be undone.')) return;
    var keys = Object.keys(COMPILER_SOURCE);
    for (var i = 0; i < keys.length; i++) {
      compilerWorkingCopy[keys[i]] = COMPILER_SOURCE[keys[i]];
    }
    try { localStorage.removeItem(STORAGE_KEYS.COMPILER_EDITS); } catch (e) {}
    if (currentEditorMode === 'compiler' && editor) {
      editor.setValue(compilerWorkingCopy[getActiveCompilerFile()] || '');
      monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);
    }
    // Revert to reference compiler
    CompilerAPI.useReference();
    updateCompilerModeIndicator();
    writeConsole('Compiler source reset to original. Using reference compiler.\n', 'output-info');
  });

  populateCompilerFileSelect();

  // ── Monaco editor ──

  function initMonaco() {
    require.config({
      paths: { vs: 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs' }
    });

    require(['vs/editor/editor.main'], function () {
      var active = getActiveFile();

      function createEditor(initialContent) {
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
          glyphMargin: true,
          renderWhitespace: 'none',
          tabSize: 4,
          padding: { top: 12, bottom: 12 }
        });

        syncSelectToActive();

        editor.onDidChangeModelContent(function () {
          scheduleAutoSave();
        });

        // Gutter click → toggle breakpoint (program mode only).
        // Only handle GUTTER_GLYPH_MARGIN (type 2) — the narrow strip to the
        // left of line numbers. Accepting GUTTER_LINE_NUMBERS (type 3) would
        // cause accidental breakpoints whenever a user clicks a line number.
        editor.onMouseDown(function (e) {
          if (!e.target) return;
          if (e.target.type !== 2) return; // 2 = GUTTER_GLYPH_MARGIN
          if (currentEditorMode !== 'program') return;
          var pos = e.target.position;
          if (!pos && e.target.range) pos = { lineNumber: e.target.range.startLineNumber };
          if (!pos) return;
          toggleBreakpoint(pos.lineNumber);
        });

        editor.addAction({
          id: 'compile-run',
          label: 'Compile & Run',
          keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter],
          run: function () {
            if (currentEditorMode === 'compiler') {
              buildCompiler();
            } else {
              compileAndRun();
            }
          }
        });

        setStatus('Ready — Ctrl+Enter to compile');
        updateCompilerModeIndicator();
      }

      if (active.type === 'user') {
        var userContent = getFileContent('user', active.id);
        if (userContent === null) {
          // File was deleted; fall back to hello example
          active = { type: 'example', id: 'hello' };
          setActiveFile('example', 'hello');
          fetchExample('hello', createEditor);
        } else {
          createEditor(userContent);
        }
      } else {
        fetchExample(active.id, createEditor);
      }
    });
  }

  // ── wabt.js (lazy-loaded) ──

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
        runBtn.textContent = 'Compiling\u2026';
        runBtn.disabled = true;
        break;
      case 'running':
        runBtn.textContent = 'Running\u2026';
        runBtn.disabled = true;
        break;
      case 'success':
        runBtn.textContent = '\u2713 Done';
        runBtn.disabled = false;
        runBtn.className = 'success';
        idleTimerId = setTimeout(function () { setButtonState('idle'); }, 2000);
        break;
      case 'error':
        runBtn.textContent = '\u2717 Error';
        runBtn.disabled = false;
        runBtn.className = 'error';
        idleTimerId = setTimeout(function () { setButtonState('idle'); }, 2000);
        break;
      default:
        runBtn.textContent = '\u25b6 Compile & Run';
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
  }

  // ── Compile & Run pipeline ──

  function compileAndRun() {
    if (!editor || isRunning) return;
    isRunning = true;

    var source = editor.getValue();
    clearOutput();
    monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);

    setButtonState('compiling');
    setStatus('Compiling…');

    CompilerAPI.compileBinary(source)
      .then(function (bytes) {
        return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
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

  // ── Debugger functions ──

  function clearBreakpoints() {
    debugBreakpoints.clear();
    if (editor) {
      breakpointDecorations = editor.deltaDecorations(breakpointDecorations, []);
    }
  }

  function toggleBreakpoint(line) {
    if (debugBreakpoints.has(line)) {
      debugBreakpoints.delete(line);
    } else {
      debugBreakpoints.add(line);
    }
    renderBreakpointDecorations();
  }

  function renderBreakpointDecorations() {
    if (!editor) return;
    var newDecors = [];
    debugBreakpoints.forEach(function (line) {
      newDecors.push({
        range: new monaco.Range(line, 1, line, 1),
        options: {
          isWholeLine: true,
          className: 'debug-breakpoint-line',
          glyphMarginClassName: 'debug-breakpoint-glyph'
        }
      });
    });
    breakpointDecorations = editor.deltaDecorations(breakpointDecorations, newDecors);
  }

  function setDebugLine(line) {
    if (!editor) return;
    debugCurrentDecorations = editor.deltaDecorations(debugCurrentDecorations, [
      {
        range: new monaco.Range(line, 1, line, 1),
        options: {
          isWholeLine: true,
          className: 'debug-current-line',
          glyphMarginClassName: 'debug-current-line-gutter'
        }
      }
    ]);
    editor.revealLineInCenterIfOutsideViewport(line);
  }

  function clearDebugLine() {
    if (!editor) return;
    debugCurrentDecorations = editor.deltaDecorations(debugCurrentDecorations, []);
  }

  function setDebugStatus(text) {
    debugStatusEl.textContent = text;
  }

  function showDebugControls() {
    debugControls.classList.remove('hidden');
  }

  function hideDebugControls() {
    debugControls.classList.add('hidden');
    setDebugStatus('');
    clearDebugLine();
  }

  function releaseDebugWorker() {
    if (!debugSabInt32) return;
    Atomics.store(debugSabInt32, DBG_PAUSE_SLOT, 1);
    Atomics.notify(debugSabInt32, DBG_PAUSE_SLOT, 1);
  }

  function debugStepInto() {
    if (!debugSabInt32) return;
    debugMode = 'step'; // pause at the very next traced statement
    releaseDebugWorker();
  }

  function debugStepOver() {
    if (!debugSabInt32) return;
    stepOverDepth = currentDepth; // only pause when depth returns to this level or shallower
    debugMode = 'step-over';
    releaseDebugWorker();
  }

  function debugContinue() {
    if (!debugSabInt32) return;
    debugMode = 'run';
    setDebugStatus('running\u2026');
    clearDebugLine();
    releaseDebugWorker();
  }

  function debugStop() {
    if (!debugSabInt32) return;
    Atomics.store(debugSabInt32, DBG_PAUSE_SLOT, 3);
    Atomics.notify(debugSabInt32, DBG_PAUSE_SLOT, 1);
  }

  function stopDebugSession() {
    if (debugWorker) {
      debugStop();
      // Give the worker a moment to self-terminate, then force-terminate
      setTimeout(function () {
        if (debugWorker) {
          debugWorker.terminate();
          debugWorker = null;
        }
      }, 300);
    }
    debugSabInt32 = null;
    isRunning = false;
    setButtonState('idle');
    hideDebugControls();
    hideTerminalInput();
  }

  function debugAndRun() {
    if (!editor || isRunning) return;
    isRunning = true;

    var source = editor.getValue();
    clearOutput();
    monaco.editor.setModelMarkers(editor.getModel(), 'c2wasm', []);

    if (typeof SharedArrayBuffer === 'undefined') {
      writeConsole(
        'SharedArrayBuffer is not available.\nRun locally with: make serve\n',
        'output-error'
      );
      isRunning = false;
      return;
    }

    setButtonState('compiling');
    setStatus('Compiling with debug instrumentation…');

    CompilerAPI.compileDebug(source)
      .then(function (bytes) {
        return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
      })
      .then(function (wasmBytes) {
        setButtonState('running');
        setStatus('Debugging…');
        showDebugControls();
        setDebugStatus('running\u2026');
        debugMode     = 'run'; // always start in run mode — only stop at breakpoints
        currentDepth  = 0;
        stepOverDepth = 0;

        // SAB: slots 0,1 for stdin; slot 2 for debug pause signal
        var sab = new SharedArrayBuffer(8 + 4096);
        var sabInt32 = new Int32Array(sab);

        debugSabInt32 = sabInt32;

        var worker = new Worker('./debug-worker.js');
        debugWorker = worker;
        activeWorker = worker;

        worker.onmessage = function (event) {
          var msg = event.data;
          if (msg.type === 'trace') {
            var line = msg.line;
            var depth = msg.depth !== undefined ? msg.depth : 0;
            currentDepth = depth;
            var hitBreakpoint = debugBreakpoints.has(line);
            if (hitBreakpoint) {
              // Always stop at breakpoints, regardless of mode or depth
              debugMode = 'run';
              setDebugLine(line);
              setDebugStatus('\u25cf line ' + line);
            } else if (debugMode === 'step') {
              // Step Into: pause at the very next traced statement
              debugMode = 'run';
              setDebugLine(line);
              setDebugStatus('line ' + line);
            } else if (debugMode === 'step-over') {
              if (depth <= stepOverDepth) {
                // Returned to the same or an outer call level — stop here
                debugMode = 'run';
                setDebugLine(line);
                setDebugStatus('line ' + line);
              } else {
                // Still inside a deeper function call — keep running
                releaseDebugWorker();
              }
            } else {
              // 'run' mode and no breakpoint — release immediately
              releaseDebugWorker();
            }
          } else if (msg.type === 'output') {
            writeConsole(msg.text);
          } else if (msg.type === 'needInput') {
            showTerminalInput(sab, sabInt32);
          } else if (msg.type === 'exit') {
            debugWorker = null;
            activeWorker = null;
            worker.terminate();
            hideTerminalInput();
            debugSabInt32 = null;
            isRunning = false;
            hideDebugControls();
            var code = msg.code;
            setButtonState(code === 0 ? 'success' : 'error');
            setStatus(code === 0 ? 'Done' : 'Exited with code ' + code);
            if (code !== 0) {
              writeConsole('\n[Process exited with code ' + code + ']', 'output-info');
            }
          } else if (msg.type === 'stopped') {
            debugWorker = null;
            activeWorker = null;
            worker.terminate();
            hideTerminalInput();
            debugSabInt32 = null;
            isRunning = false;
            hideDebugControls();
            setButtonState('idle');
            setStatus('Stopped');
            writeConsole('\n[Debugger stopped]', 'output-info');
          } else if (msg.type === 'error') {
            debugWorker = null;
            activeWorker = null;
            worker.terminate();
            hideTerminalInput();
            debugSabInt32 = null;
            isRunning = false;
            hideDebugControls();
            writeConsole(msg.message, 'output-error');
            setButtonState('error');
            setStatus('Error');
          }
        };

        worker.onerror = function (e) {
          if (debugWorker) {
            debugWorker = null;
            activeWorker = null;
            worker.terminate();
          }
          hideTerminalInput();
          debugSabInt32 = null;
          isRunning = false;
          hideDebugControls();
          writeConsole(e.message || 'Worker error', 'output-error');
          setButtonState('error');
          setStatus('Error');
        };

        // Transfer wasmBytes to the worker (zero-copy)
        worker.postMessage({ type: 'run', wasmBytes: wasmBytes, sab: sab }, [wasmBytes]);
      })
      .catch(function (err) {
        var msg = err.message || String(err);
        var errors = parseErrors(msg);
        if (errors.length > 0) setErrorMarkers(errors);
        writeConsole(msg, 'output-error');
        setButtonState('error');
        setStatus('Error');
        isRunning = false;
        hideDebugControls();
      });
  }

  // ── Bind events ──

  runBtn.addEventListener('click', compileAndRun);
  debugBtn.addEventListener('click', debugAndRun);
  debugStepIntoBtn.addEventListener('click', debugStepInto);
  debugStepOverBtn.addEventListener('click', debugStepOver);
  debugContinueBtn.addEventListener('click', debugContinue);
  debugStopBtn.addEventListener('click', stopDebugSession);

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
  // Pre-fetch the hello example so fallback paths always have it cached,
  // then hand off to Monaco initialization (which will fetch the actual
  // active example if needed).
  fetchExample('hello', function () { initMonaco(); });
})();
