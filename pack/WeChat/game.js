// WeChat Mini-Game Entry Point for MyRenderer
// Loads the Emscripten-generated JS glue and initializes the game.
// Place this in the mini-game project root alongside the .wasm/.js files.

// WeChat mini-game environment provides a global canvas.
// Our Emscripten build uses "#canvas" selector — we need to create
// the canvas element for WeChat's runtime.

// Store the real wx API on the global scope (Emscripten may override it)
var realWx = wx;

// Create canvas for WebGL rendering
var canvas = wx.createCanvas();
canvas.id = 'canvas';

// WeChat requires the canvas to be in the document-like environment
// (wx.createCanvas() already provides the correct canvas for WebGL)

// Import the Emscripten JS glue (this initializes the wasm module)
// The glue script name should match the build output.
// E.g. if building MiniGame-Mesh, rename to "game.wasm.js"
require('./mxrender.js');
