// MyRenderer — WeChat (WeiXin) Mini-Game Entry
// ===============================================
// Same 5-step structure as Douyin/game.js.
// Only difference: wx.* instead of tt.*.
// See pack/Douyin/game.js for detailed inline comments.

console.log('[game.js] WeChat MiniGame starting...');

// ---- Step 1: Save platform API ----
var realWx = wx;

// ---- Step 2: Create canvas ----
var canvas = realWx.createCanvas();
canvas.id = 'canvas';
console.log('[game.js] Canvas created: ' + canvas.width + 'x' + canvas.height);

// ---- Step 3: Load shared polyfills + inject canvas ----
// Set canvas BEFORE polyfills — polyfills reads __mx_canvas internally
globalThis.__mx_canvas = canvas;
require('./polyfills.js');

// ---- Step 3b: Fix fetch for local WASM loading ----
globalThis.fetch = function(url) {
  return new Promise(function(resolve, reject) {
    realWx.getFileSystemManager().readFile({
      filePath: typeof url === 'string' ? url.replace(/^\.?\//, '') : 'mxrender.wasm',
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

// ---- Step 3c: Force navigator.userAgent override ----
try {
  Object.defineProperty(globalThis.navigator, 'userAgent', {
    value: 'Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/605.1.15',
    writable: true, configurable: true
  });
  console.log('[game.js] userAgent overridden');
} catch(e) {
  console.log('[game.js] userAgent override failed:', e.message);
}

// ---- Step 4: Configure Emscripten Module ----
var Module = {
  canvas: canvas,
  wasmBinaryFile: 'mxrender.wasm',
  noImageDecoding: true,
  noAudioDecoding: true,
  print: function(t)    { console.log('[wasm]', t); },
  printErr: function(t) { console.error('[wasm]', t); },
  onAbort: function(m)  { console.error('[wasm] ABORT:', m); },

  postRun: [function() {
    console.log('[game.js] WASM ready, registering input bridge');
    realWx.onTouchStart(function(e) {
      if (!e.touches || !e.touches.length) return;
      var t = e.touches[0];
      Module._mx_feed_mouse_move(t.clientX, t.clientY);
      Module._mx_feed_mouse_down(0);
    });
    realWx.onTouchMove(function(e) {
      if (!e.touches || !e.touches.length) return;
      var t = e.touches[0];
      Module._mx_feed_mouse_move(t.clientX, t.clientY);
    });
    realWx.onTouchEnd(function(e) {
      Module._mx_feed_mouse_up(0);
    });
    realWx.onTouchCancel(function(e) {
      Module._mx_feed_mouse_up(0);
    });
    console.log('[game.js] Input bridge ready');
  }],
};

// ---- Step 5: Load Emscripten glue ----
console.log('[game.js] Loading mxrender.js...');
require('./mxrender.js');
console.log('[game.js] Done.');
