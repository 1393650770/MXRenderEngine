// Strip require('node:*') calls from Emscripten-generated JS
// Mini-game bundlers fail on these — they statically resolve all require() calls.
// Usage: node strip_node_requires.js <mxrender.js>
var fs = require('fs');
var path = process.argv[2];
if (!path) { console.error('Usage: node strip_node_requires.js <file>'); process.exit(1); }
var content = fs.readFileSync(path, 'utf8');
var before = (content.match(/node:crypto|node:fs|node:path/g) || []).length;
content = content.replace(/^.*require\('node:[^']+'\);?\r?\n/gm, '');
var after = (content.match(/node:crypto|node:fs|node:path/g) || []).length;
fs.writeFileSync(path, content);
console.log('[strip] ' + path + ': ' + before + ' node:* refs removed, ' + after + ' remaining');
