// polyfills.js — MyRenderer Mini-Game Emscripten Adapter
// =======================================================
// Shared between Douyin and WeChat mini-game environments.
// Provides browser API mocks that Emscripten's glue code expects.
//
// MUST be loaded BEFORE mxrender.js.
// Platform entry MUST set globalThis.__mx_canvas before loading this.

(function() {
  'use strict';

  // ====================================================================
  // PRIVATE STATE
  // ====================================================================
  var _g = Function('return this')();   // true global (not globalThis proxy)
  // Read canvas from globalThis.__mx_canvas (set by game.js before loading this)
  var _canvas = _g.__mx_canvas || null;

  // ====================================================================
  // 1. globalThis polyfill
  //    Emscripten checks globalThis.WebAssembly, globalThis.Int32Array, etc.
  // ====================================================================
  _g.globalThis = _g;

  // ====================================================================
  // 2. window polyfill
  //    Emscripten references window.location, window.document, etc.
  //    Must have addEventListener for emscripten_set_resize_callback
  //    (targets EMSCRIPTEN_EVENT_TARGET_WINDOW).
  // ====================================================================
  if (typeof _g.window === 'undefined') {
    _g.window = _g;
  }
  // Ensure addEventListener exists on window/globalThis (mini-game may not provide it).
  // emscripten_set_resize_callback targets EMSCRIPTEN_EVENT_TARGET_WINDOW
  // which resolves to window/globalThis and calls addEventListener on it.
  if (!_g.window.addEventListener) {
    _g.window.addEventListener    = function(type, handler, opts) {};
    _g.window.removeEventListener = function(type, handler, opts) {};
  }
  if (!_g.addEventListener) {
    _g.addEventListener    = _g.window.addEventListener;
    _g.removeEventListener = _g.window.removeEventListener;
  }

  // ====================================================================
  // 3. TypedArray → globalThis
  //    mxrender.js assertion line ~520:
  //    "assert(globalThis.Int32Array && globalThis.Float64Array,
  //            'JS engine does not provide full typed array support')"
  //    In mini-game, constructors exist globally but NOT on globalThis.
  // ====================================================================
  var TA = [
    'Int8Array', 'Uint8Array', 'Uint8ClampedArray',
    'Int16Array', 'Uint16Array',
    'Int32Array', 'Uint32Array',
    'Float32Array', 'Float64Array',
    'BigInt64Array', 'BigUint64Array'
  ];
  for (var i = 0; i < TA.length; i++) {
    var n = TA[i];
    if (typeof _g[n] !== 'undefined' && typeof _g.globalThis[n] === 'undefined') {
      _g.globalThis[n] = _g[n];
    }
  }

  // ====================================================================
  // 4. WebAssembly → globalThis
  //    mxrender.js line ~255: "typeof globalThis.WebAssembly !== 'undefined'"
  // ====================================================================
  if (typeof _g.WebAssembly !== 'undefined' && typeof _g.globalThis.WebAssembly === 'undefined') {
    _g.globalThis.WebAssembly = _g.WebAssembly;
  }

  // ====================================================================
  // 5. performance.now()
  //    EmscriptenGLWindow::GetTime() → emscripten_get_now() → performance.now()
  //    Mini-game has Date.now() but not performance.now().
  // ====================================================================
  if (typeof _g.performance === 'undefined') {
    _g.performance = {};
  }
  if (typeof _g.performance.now !== 'function') {
    _g.performance.now = function() { return Date.now(); };
  }

  // ====================================================================
  // 6. document polyfill — CRITICAL PATH
  //    EmscriptenInit: emscripten_set_canvas_element_size("#canvas", w, h)
  //      → calls document.getElementById("canvas") or document.querySelector("#canvas")
  //    Browser.init(): document.createElement('canvas'), document.body.appendChild
  //    PointerLock/Fullscreen: document.addEventListener, document.exitPointerLock, etc.
  // ====================================================================
  var doc = {
    // -- Canvas lookup (hot path — every resize) --
    getElementById: function(id) {
      return (id === 'canvas' || id === '#canvas') ? _canvas : null;
    },
    querySelector: function(sel) {
      return (sel === '#canvas' || sel === 'canvas') ? _canvas : null;
    },
    // -- Canvas creation fallback (cold path — only if Module.canvas is null) --
    createElement: function(tag) {
      return _canvas;
    },
    createElementNS: function(ns, tag) {
      return _canvas;
    },
    // -- Body (used to append canvas into DOM) --
    body: {
      appendChild: function(el) {},
      removeChild: function(el) {},
      insertBefore: function(el, ref) {},
    },
    // -- Event system (stubbed — mini-game has no DOM events) --
    addEventListener: function(type, handler, opts) {},
    removeEventListener: function(type, handler, opts) {},
    dispatchEvent: function(evt) {},
    // -- Fullscreen (stubbed — not available in mini-game) --
    get fullscreenElement() { return null; },
    exitFullscreen: function() {},
    requestFullscreen: function() {},
    fullscreenEnabled: false,
    // -- Pointer lock (stubbed) --
    get pointerLockElement() { return null; },
    exitPointerLock: function() {},
    // -- Visibility / focus (stubbed) --
    hidden: false,
    visibilityState: 'visible',
    hasFocus: function() { return true; },
    // -- Misc —
    createDocumentFragment: function() { return { appendChild: function() {} }; },
    documentElement: { style: {} },
  };
  _g.document = doc;

  // ====================================================================
  // 7. navigator polyfill
  //     MUST override userAgent to pass Emscripten's minimum browser check.
  //     Mini-game runtime reports old Safari version (v13.x) which fails
  //     the >= v15.0.0 requirement. Object.defineProperty handles read-only
  //     getters that prevent simple assignment.
  // ====================================================================
  if (typeof _g.navigator === 'undefined') _g.navigator = {};
  var nav = _g.navigator;
  var FAKE_UA = 'Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/605.1.15';
  try {
    Object.defineProperty(_g.navigator, 'userAgent', {
      value: FAKE_UA, writable: true, configurable: true
    });
  } catch(e) {
    // Fallback: simple assignment (works if property is not protected)
    _g.navigator.userAgent = FAKE_UA;
  }
  if (!nav.platform)           nav.platform = '';
  if (!nav.hardwareConcurrency) nav.hardwareConcurrency = 4;
  if (!nav.mediaDevices)       nav.mediaDevices = { getUserMedia: function() {} };

  // ====================================================================
  // 8. URL mock
  //    Emscripten uses URL.createObjectURL for blob URLs (images/audio).
  //    Module['noImageDecoding'] skips this, but mock the reference.
  // ====================================================================
  if (typeof _g.URL === 'undefined') {
    _g.URL = {
      createObjectURL: function(blob) { return ''; },
      revokeObjectURL: function(url) {},
    };
  }

  // ====================================================================
  // 9. Blob constructor mock
  // ====================================================================
  if (typeof _g.Blob === 'undefined') {
    _g.Blob = function(parts, opts) {};
  }

  // ====================================================================
  // 10. Image constructor mock
  //     Browser.init() preloads images via new Image().
  //     Module['noImageDecoding']=true skips the preload logic,
  //     but the constructor reference is still checked.
  // ====================================================================
  if (typeof _g.Image === 'undefined') {
    _g.Image = function(w, h) {
      this.width = w || 0;
      this.height = h || 0;
      this.src = '';
      this.onload = null;
      this.onerror = null;
      this.complete = false;
      this.naturalWidth = 0;
      this.naturalHeight = 0;
    };
  }

  // ====================================================================
  // 11. Audio constructor mock
  //     Module['noAudioDecoding']=true skips preload.
  // ====================================================================
  if (typeof _g.Audio === 'undefined') {
    _g.Audio = function(src) {
      this.src = src || '';
      this.play = function() {};
      this.pause = function() {};
      this.load = function() {};
      this.volume = 1.0;
      this.loop = false;
      this.currentTime = 0;
      this.addEventListener = function() {};
    };
  }

  // ====================================================================
  // 12. XMLHttpRequest mock
  //     Emscripten's getBinaryPromise() uses XHR as wasmBinary fallback.
  //     Not needed when Module.wasmBinary is set, but class must exist.
  // ====================================================================
  if (typeof _g.XMLHttpRequest === 'undefined') {
    _g.XMLHttpRequest = function() {
      this.status = 0;
      this.response = null;
      this.responseType = '';
      this.onload = null;
      this.onerror = null;
      this.open = function(method, url, async) {};
      this.send = function(data) {};
      this.setRequestHeader = function(key, val) {};
      this.overrideMimeType = function(mime) {};
      this.getAllResponseHeaders = function() { return ''; };
    };
  }

  // ====================================================================
  // 13. fetch mock
  //     Emscripten's instantiateAsync default path.
  //     Not needed with Module.wasmBinary, but class must exist.
  // ====================================================================
  if (typeof _g.fetch === 'undefined') {
    _g.fetch = function(url, opts) {
      return Promise.reject(new Error('fetch not available in mini-game'));
    };
  }

  // ====================================================================
  // 14. TextDecoder
  //     Emscripten's UTF8ArrayToString uses TextDecoder.
  //     Mini-game runtimes typically have it, but ensure fallback.
  // ====================================================================
  if (typeof _g.TextDecoder === 'undefined') {
    _g.TextDecoder = function(encoding) {
      this.decode = function(buffer) {
        if (!buffer) return '';
        var bytes = new Uint8Array(buffer.buffer || buffer, buffer.byteOffset || 0, buffer.byteLength || buffer.length);
        var str = '';
        var i = 0;
        while (i < bytes.length) {
          var c = bytes[i++];
          if (c < 128) { str += String.fromCharCode(c); }
          else if (c < 224) { str += String.fromCharCode(((c & 0x1F) << 6) | (bytes[i++] & 0x3F)); }
          else { str += String.fromCharCode(((c & 0x0F) << 12) | ((bytes[i++] & 0x3F) << 6) | (bytes[i++] & 0x3F)); }
        }
        return str;
      };
    };
  }

  // ====================================================================
  // 15. screen mock
  //     Emscripten references screen.width/height for default canvas size.
  // ====================================================================
  if (typeof _g.screen === 'undefined') {
    _g.screen = { width: 375, height: 667, availWidth: 375, availHeight: 667, colorDepth: 24, pixelDepth: 24 };
  }

  // ====================================================================
  // 16. location mock
  // ====================================================================
  if (typeof _g.location === 'undefined') {
    _g.location = { href: '', protocol: 'https:', host: '', hostname: '', pathname: '/', search: '', hash: '', origin: 'https://localhost' };
  }

  // ====================================================================
  // 17. Timers (usually available, but verify)
  // ====================================================================
  if (typeof _g.setTimeout === 'undefined') {
    _g.setTimeout = function(fn, delay) { return 0; };
    _g.clearTimeout = function(id) {};
  }

  // ====================================================================
  // 18. console (usually available, but verify)
  // ====================================================================
  if (typeof _g.console === 'undefined') {
    _g.console = { log: function(){}, error: function(){}, warn: function(){} };
  }

  // Keep screen dimensions in sync with canvas
  if (_canvas) {
    _g.screen.width = _canvas.width;
    _g.screen.height = _canvas.height;
    _g.screen.availWidth = _canvas.width;
    _g.screen.availHeight = _canvas.height;
  }

  console.log('[polyfills.js] Mini-game environment ready');
})();
