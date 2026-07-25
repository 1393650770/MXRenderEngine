// Douyin (TikTok) Mini-Game Entry Point for MyRenderer
// Loads the Emscripten-generated JS glue and initializes the game.
// Douyin uses tt.* APIs (vs WeChat's wx.*).

var realTT = tt;

// Create canvas for WebGL rendering
var canvas = tt.createCanvas();
canvas.id = 'canvas';

// Load the Emscripten JS glue
require('./mxrender.js');
