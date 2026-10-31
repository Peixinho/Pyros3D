# Pyros3D Game Engine
[![Build](https://github.com/Peixinho/Pyros3D/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/Peixinho/Pyros3D/actions/workflows/build.yml)

Pyros3D is a work in progress game engine, focused on 3D, but that has 2 projects launched on android in 2D.
- [Bang! Bang! Bunny](https://play.google.com/store/apps/details?id=com.madblowfish.bangbangbunny)
- [Bang! Bang! Judas](https://play.google.com/store/apps/details?id=com.madblowfish.bangbangjudas)

You can compile for Windows, Linux and macOS (Vulkan or Metal), the Web (Emscripten / WebGPU), and Android and Raspberry Pi (Vulkan).

After clone: `git submodule update --init --recursive` (imgui + **box3d**).

## Rendering

One renderer (`src/Pyros3D/Rendering/Renderer`) on a thin device layer (`src/Pyros3D/Rendering/Device`) with three
backends: **Metal** (macOS), **Vulkan** (Windows, Linux, Android, Raspberry Pi; MoltenVK on macOS) and **WebGPU**
(Dawn natively, the browser's through Emscripten). Physically based lighting with cascaded and local-light shadows,
a physical sky with volumetric clouds (multiple scattering: a grey overcast, not a blue one) and their shadows, haze,
weather a game sets (cloud cover, haze, rain - the world soaking and gathering puddles out in it, dry under cover), image-based ambient (sky, spherical harmonics, a
solved DDGI volume, reflection probes), ground-truth AO, screen-space reflections, ray-traced sun shadows and reflections (through the scene's BVH, on every backend - no ray-tracing hardware needed), temporal antialiasing, bloom, light shafts, depth of field, motion blur,
automatic exposure and a filmic curve, a render scale resolved up by the temporal AA, HDR output (Metal, EDR), particles (a million at once simulated on the GPU), automatic instancing and levels of detail, materials written as
surface functions (the Material Editor's node graph compiles to one), wet surfaces and puddles, light under the skin
(subsurface), plants that sway in the wind and leaves that let the sun through, and second views drawn into textures
(`SceneView`: scopes, monitors, mirrors). The renderer's shaders (`resources/shaders`) are compiled to SPIR-V when
the engine is built and embedded in it.

## Dependencies
- **Box3D** (physics) — git submodule `src/Pyros3D/Ext/box3d` (@ v0.1.0). Same backend everywhere (`BOX3D_DISABLE_SIMD` under emcmake).
- **glslc** (shaderc) to build: the renderer's shaders. **SPIRV-Cross** for Metal. **glslang** (optional) to compile materials' surfaces at run time.
- Freetype 2.6+
- Assimp 3.0 (to build tools/AssimImporter that converts regular 3D models (obj,dae, ...) to pyros format)
- SDL2 (the window, everywhere)
- Lua 5.1–5.4 (optional, for Lua bindings / DemoLauncher / the editor)

To build natively, run CMake (see `cmake/PyrosOptions.cmake`: `PYROS_GRAPHICS` is `Metal`, `Vulkan` or `WebGPU`,
defaulting to Metal on Apple and Vulkan elsewhere). Requires **CMake 3.22+**.

```bash
# macOS: brew install cmake ninja sdl2 freetype lua@5.4 shaderc spirv-cross glslang assimp
cmake -S . -B build -G Ninja -DBUILD_DEMOS=ON -DBUILD_EDITOR=ON
cmake --build build -j
```

The native WebGPU device is built too when a Dawn build is found (see `cmake/PyrosWebGPU.cmake`);
`PYROS_GFX_BACKEND=webgpu|metal|vulkan` then picks the device at launch.

## Windows (MSVC)

Vulkan, DLL or static. Dependencies come from [vcpkg](https://vcpkg.io); the LunarG Vulkan SDK supplies the
headers, `glslc` and glslang.

`vcpkg.json` at the repo root declares the dependency set, so the toolchain
installs them for you — no separate `vcpkg install` needed. It pins Lua to
5.4.x on purpose: the current port is 5.5, which the vendored `sol.hpp` does
not support.

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DPYROS_GRAPHICS=Vulkan `
  -DBUILD_DEMOS=ON -DBUILD_EDITOR=ON
cmake --build build --parallel
```

Keep the installed Vulkan SDK at or above the `volk` tag pinned in
`cmake/PyrosBackend.cmake` — volk references entry points declared in the
Vulkan headers of its own release.

`PYROS_ASSET_ROOT` controls where DemoLauncher and PyrosBuilder look for
their Lua assets. It defaults to the absolute source path, which is right
for a local build; pass `-DPYROS_RELOCATABLE_ASSETS=ON` for a redistributable
one and ship `assets/` next to the executable.

Prebuilt artifacts are produced by
[`.github/workflows/windows.yml`](.github/workflows/windows.yml) on every push.

## Emscripten / WebGPU

Use **emcmake** (WebGPU + SDL2 + **Box3D**). The browser's WebGPU comes through emdawnwebgpu, built once from Dawn's
source; the shaders' WGSL is made at build time by a native tool (`pyros_spirv2wgsl`) - see `cmake/PyrosWebGPU.cmake`.

See [otherplatforms/emscripten/README.md](otherplatforms/emscripten/README.md).

```bash
git submodule update --init --recursive
emcmake cmake -S . -B build_web -G "Unix Makefiles" -DBUILD_DEMOS=ON
cmake --build build_web -j --target Pyros3D
# serve build_web/examples/web and open index.html (JS owns the engine; needs a browser with WebGPU)
```

Physics from JS: `new P.Box3DPhysics()` (Embind). DemoLauncher “Simple Physics” on native uses the same `Physics` → `Box3DPhysics` path.

## Raspberry Pi (Vulkan)

Pi 4 and 5 through Mesa's Vulkan driver (v3dv) + SDL2, built on the device.
See [otherplatforms/raspberry-pi/README.md](otherplatforms/raspberry-pi/README.md).

```bash
cmake -S . -B build_rpi -G Ninja -DPYROS_GRAPHICS=Vulkan -DBUILD_DEMOS=ON -DSTATIC_LIB=ON
cmake --build build_rpi -j
```

## Android (Vulkan + SDL2)

NDK CMake scaffolding (engine static lib). **Not tested on device yet.**
See [otherplatforms/android/README.md](otherplatforms/android/README.md).

## Running Example
[Legacy WebGL JS demo](https://www.duartepeixinho.com/pyrosjs/) — new path is the emcmake `Pyros3D` library + `examples/web/`.

## Screenshots
![Rotating Cube](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/RotatingCube/Rotating%20Cube.png)
![Rotating Textured Cube](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/RotatingTexturedCube/RotatingTexturedCube.png)
![Island Demo](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/IslandDemo/IslandDemo.png)
![Shadows](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/RotatingCubeWithLightingAndShadow/Rotating%20Cube%20With%20Lighting%20And%20Shadows.png)
![Physics](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/SimplePhysics/Simple%20Physics%20Example.png)
![Skeleton Animation](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/SkeletonAnimationExample/SkeletonAnimation.png)
![Text Rendering](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/TextRendering/TextRendering.png)
![Decals](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/Decals/Decals.png)

License
----

MIT
