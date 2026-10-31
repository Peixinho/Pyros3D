# Emscripten / WebGPU

Web builds use the main CMake tree via **emcmake**, targeting **WebGPU** and **SDL2**.

The primary web product is a **JS-first library**: load `Pyros3D.js`, then create the window, scene, meshes, and `requestAnimationFrame` loop entirely in JavaScript (same idea as the old [pyrosjs](https://www.duartepeixinho.com/pyrosjs/) page).

## Prerequisites

1. [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (`emsdk`) activated in the shell (`emcc` / `emcmake` on `PATH`).
2. Submodules initialized (`git submodule update --init --recursive`) — **imgui** + **box3d**.
3. Emscripten ports pull SDL2 + FreeType on first configure. Physics is the Box3D submodule (compiled into the wasm; SIMD disabled under Emscripten via `BOX3D_DISABLE_SIMD`).

## Configure & build

```bash
# from repo root, with emsdk/homebrew emscripten on PATH
emcmake cmake -S . -B build_web -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_DEMOS=ON

cmake --build build_web -j --target Pyros3D
```

Forced by `cmake/PyrosEmscripten.cmake` when `EMSCRIPTEN` is set:

| Option | Value |
|--------|--------|
| `PYROS_GRAPHICS` | `WebGPU` |
| `PYROS_WEB_WEBGPU` | `ON` |
| `BUILD_VULKAN_BACKEND` / `BUILD_METAL_BACKEND` | `OFF` |
| `STATIC_LIB` | `ON` |
| `HAVE_LUA_BINDINGS` | `OFF` (default) |

Optional: `-DHAVE_LUA_BINDINGS=ON` also builds `DemoLauncher` (FetchContent Lua 5.4).

## Run (JS API)

```bash
# must be served over HTTP (not file://)
python3 -m http.server -d build_web/examples 8765
# open http://127.0.0.1:8765/web/index.html
```

| Artifact | Role |
|----------|------|
| `build_web/examples/Pyros3D.js` (+ `.wasm` / `.data`) | MODULARIZE factory `createPyros3D({ canvas })` |
| `build_web/examples/web/index.html` + `main.js` | Neon Pulse (JS port of `assets/neonpulse`) |
| `CppApiDemo.html` | Optional C++-owned demo (ImGui), if built |

Minimal client:

```js
const P = await createPyros3D({ canvas: document.querySelector("#canvas") });
// (awaited: the window waits for the browser's WebGPU adapter and device)
const app = await new P.Application(1280, 800, "Pyros3D", P.WindowType_Close | P.WindowType_Resize);
await app.init();
const renderer = app.createRenderer();   // the engine's renderer, drawing on the app's canvas
const scene = new P.Scene();
// … camera (a GameObject), projection.perspective(...), meshes / lights / materials …
function frame() {
  app.pollEvents();
  scene.update(app.getTime());
  renderer.renderScene(projection, camera, scene);   // draws and presents the frame
  app.draw();
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
```

API parity with Lua is tracked in `EMBIND_PARITY.md`.

## Notes

- Physics is **Box3D** (same as native): `new P.Box3DPhysics()` from Embind; linked from the `src/Pyros3D/Ext/box3d` submodule.
- The browser needs WebGPU (Chrome/Edge 113+, Safari 26+, Firefox 141+ on Windows). `renderer.useSceneSettings(scene)` takes a loaded editor scene's ambient, background, sky, reflections and exposure; `setSky`, `setHaze`, `setClouds`, `setReflections`, `setAutoExposure`, `setExposure`, `setTemporalAA`, `setBackground`, `setGlobalLight` set them by hand.
- miniaudio uses the browser Web Audio backend (ScriptProcessorNode today). Chrome may log a deprecation warning; AudioWorklet needs ASYNCIFY/pthreads and breaks Embind sync APIs — left for a later miniaudio upgrade.
- Embind is linked (`-lembind`); expand `PyrosEmbind.cpp` toward full Lua surface.
- `Pyros3D.data` packs shaders + `verdana.ttf` + `neonpulse/sfx`. Game textures should use HTTP:
  ```js
  import { installPyrosAssets } from "./pyros-assets.js";
  installPyrosAssets(P);
  await tex.loadFromUrl("./assets/foo.png");           // or P.loadTextureFromUrl(url)
  // tex.loadTextureFromMemory(uint8Array)             // Embind, sync
  ```
  `fopen` / `loadTexture(path)` still only sees the VFS (`.data` or `FS.writeFile`).
