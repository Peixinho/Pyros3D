# The GI demo, built for the web

`index.html` + `GIDemo.js` + `GIDemo.wasm`, and nothing else: the shaders
are embedded in the wasm and the Cornell box is built in code, so this
folder is self-contained and can be served from anywhere that serves
static files.

To put it on the internet from this repository, turn on GitHub Pages
with **Settings -> Pages -> Source: Deploy from a branch**, branch
`compute-gl45` (or whatever it has been merged into), folder `/docs`.
The demo is then at `<pages-url>/gi-demo/`.

To look at it locally:

    python3 -m http.server -d docs/gi-demo 8000

and open <http://localhost:8000>. A plain `file://` open will not work -
browsers refuse to fetch the wasm over that scheme.

## Rebuilding it

    emcmake cmake -S . -B build_web -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=ON -DHAVE_LUA_BINDINGS=OFF
    cmake --build build_web --target GIDemo
    cp build_web/examples/GIDemo.js build_web/examples/GIDemo.wasm docs/gi-demo/

The binaries are committed because the point of this folder is to be
servable without a build. They are ~1.7MB together.

## One thing that did not work

Publishing this as a Claude artifact. The page renders, the supporting
files are served with the right types, and `WebAssembly.compileStreaming`
succeeds - but the frame goes blank the moment `GIDemo.js` is included,
taking the static HTML with it. The same three files work from any plain
static server. Not worth further guessing from outside a cross-origin
sandbox with no console.
