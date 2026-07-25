// MyRenderer — Douyin (TikTok) Mini-Game Entry
// ===============================================
// 5-step sequence:
//   1. Save tt API
//   2. Create canvas via tt.createCanvas()
//   3. Load shared polyfills + inject canvas
//   4. Configure Module (wasmBinary, postRun input bridge)
//   5. require('./mxrender.js')

console.log('[game.js] Douyin MiniGame starting...');

// ---- Step 1: Save platform API ----
var realTT = tt;

// ---- Step 2: Create canvas ----
var canvas = realTT.createCanvas();
canvas.id = 'canvas';
// Lock canvas size — C++ EmscriptenGLWindow constructor calls
// emscripten_set_canvas_element_size(1280, 960) which would overwrite
// the phone's native resolution. Object.defineProperty prevents this.
Object.defineProperty(canvas, 'width',  { value: canvas.width,  writable: false, configurable: true });
Object.defineProperty(canvas, 'height', { value: canvas.height, writable: false, configurable: true });
console.log('[game.js] Canvas created: ' + canvas.width + 'x' + canvas.height + ' (locked)');

// ---- Step 3: Load shared polyfills + inject canvas ----
// Set canvas BEFORE polyfills — polyfills reads __mx_canvas internally
globalThis.__mx_canvas = canvas;
require('./polyfills.js');

// ---- Step 3b: Fix fetch for local WASM loading ----
// polyfills.js provides a stub fetch (returning reject).
// Replace it with a working implementation using FileSystemManager.
globalThis.fetch = function(url) {
  // Map ANY .wasm request to mxrender.wasm (Emscripten infers filename
  // from script name but the actual file is always mxrender.wasm)
  var filePath = (typeof url === 'string' && /\.wasm/.test(url)) ? 'mxrender.wasm' : url;
  filePath = filePath.replace(/^\.?\//, '');
  return new Promise(function(resolve, reject) {
    realTT.getFileSystemManager().readFile({
      filePath: filePath,
      success: function(res) {
        resolve({
          ok: true, status: 200,
          arrayBuffer: function() { return Promise.resolve(res.data); },
        });
      },
      fail: function(err) { reject(new Error('fetch failed: ' + JSON.stringify(err))); }
    });
  });
};
console.log('[game.js] fetch polyfill ready');

// ---- Step 3c: Fix window for Emscripten resize callback ----
// Emscripten calls window.addEventListener for resize events.
// Mini-game's window may not have addEventListener.
if (typeof window !== 'undefined') {
  window.addEventListener    = window.addEventListener    || function(){};
  window.removeEventListener = window.removeEventListener || function(){};
  window.dispatchEvent       = window.dispatchEvent       || function(){};
}
if (typeof globalThis !== 'undefined' && globalThis !== window) {
  globalThis.addEventListener    = globalThis.addEventListener    || function(){};
  globalThis.removeEventListener = globalThis.removeEventListener || function(){};
}
console.log('[game.js] window event methods patched');

// ---- Step 3d: Force navigator.userAgent override ----
// Emscripten's minimum browser check requires Safari >= v15.0.0.
// Mini-game runtime reports old Safari version even if UA is overridden
// in polyfills (runtime may protect the property with a getter).
// Object.defineProperty forces the override past any protection.
try {
  Object.defineProperty(globalThis.navigator, 'userAgent', {
    value: 'Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/605.1.15',
    writable: true, configurable: true
  });
  console.log('[game.js] userAgent overridden: ' + globalThis.navigator.userAgent.substring(0, 30) + '...');
} catch(e) {
  console.log('[game.js] userAgent override failed:', e.message);
}

// ---- Step 4: Configure Emscripten Module ----
// In mini-game, each module has its own isolated global.
// We intercept WebAssembly.instantiate to capture WASM exports
// into OUR Module, bypassing the scope isolation.
var _origInstantiate = WebAssembly.instantiate;
WebAssembly.instantiate = function(buf, imports) {
  return _origInstantiate.call(WebAssembly, buf, imports).then(function(result) {
    var e = result.instance.exports;
    // Map WASM exports to our Module so touch handlers can call them
    if (e.mx_feed_mouse_move) Module._mx_feed_mouse_move = function(x,y){ e.mx_feed_mouse_move(x,y); };
    if (e.mx_feed_mouse_down)  Module._mx_feed_mouse_down  = function(b){ e.mx_feed_mouse_down(b); };
    if (e.mx_feed_mouse_up)    Module._mx_feed_mouse_up    = function(b){ e.mx_feed_mouse_up(b); };
    if (e.mx_feed_scroll)      Module._mx_feed_scroll      = function(d){ e.mx_feed_scroll(d); };
    if (e.mx_set_canvas_size)  Module._mx_set_canvas_size  = function(w,h){ e.mx_set_canvas_size(w,h); };
    console.log('[game.js] WASM exports intercepted');
    return result;
  });
};

var Module = {
  canvas: canvas,
  wasmBinaryFile: 'mxrender.wasm',
  noImageDecoding: true,
  noAudioDecoding: true,
  print: function(t)    { console.log('[wasm]', t); },
  printErr: function(t) { console.error('[wasm]', t); },
  onAbort: function(m)  { console.error('[wasm] ABORT:', m); },

};

// ---- Step 5: Load Emscripten glue ----
console.log('[game.js] Loading mxrender.js...');
require('./mxrender.js');

// ---- Input bridge: touch → mouse emulation ----
// Module is now on globalThis (set in Step 4), so mxrender.js shares
// the same Module object and _mx_feed_* exports will be assigned here.
// Until WASM init completes, they are stubs that throw — try/catch
// silently handles the transition.
realTT.onTouchStart(function(e) {
  if (!e.touches || !e.touches.length) return;
  var t = e.touches[0];
  console.log('[touch]', t.clientX, t.clientY);
  try { Module._mx_feed_mouse_move(t.clientX, t.clientY); } catch(e) { console.log('[touch] err:', e.message); }
  try { Module._mx_feed_mouse_down(0); } catch(e) {}
});
realTT.onTouchMove(function(e) {
  if (!e.touches || !e.touches.length) return;
  var t = e.touches[0];
  try { Module._mx_feed_mouse_move(t.clientX, t.clientY); } catch(e) {}
});
realTT.onTouchEnd(function(e) {
  try { Module._mx_feed_mouse_up(0); } catch(e) {}
});
realTT.onTouchCancel(function(e) {
  try { Module._mx_feed_mouse_up(0); } catch(e) {}
});
console.log('[game.js] Input bridge registered');

console.log('[game.js] Done.');
