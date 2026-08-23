# Pyros3D Game Engine
[![Build](https://github.com/Peixinho/Pyros3D/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/Peixinho/Pyros3D/actions/workflows/build.yml)

Pyros3D is a work in progress game engine, focused on 3D, but that has 2 projects launched on android in 2D.
- [Bang! Bang! Bunny](https://play.google.com/store/apps/details?id=com.madblowfish.bangbangbunny)
- [Bang! Bang! Judas](https://play.google.com/store/apps/details?id=com.madblowfish.bangbangjudas)

You can compile for Windows, Linux, MacOSX, Android, Raspberry Pi (GLES3), and Web (Emscripten / WebGL2).

After clone: `git submodule update --init --recursive` (imgui + **box3d**).

## Dependencies
- **Box3D** (physics) — git submodule `src/Pyros3D/Ext/box3d` (@ v0.1.0). Same backend on desktop, Raspberry Pi, Android, and **Emscripten / WebGL2** (`BOX3D_DISABLE_SIMD` under emcmake).
- Freetype 2.6+
- Assimp 3.0 (to build tools/AssimImporter that converts regular 3D models (obj,dae, ...) to pyros format)
- SDL2 (examples / DemoLauncher)
- Lua 5.1–5.4 (optional, for Lua bindings / DemoLauncher)

To build natively, run CMake (see also `cmake/PyrosOptions.cmake` for `PYROS_GRAPHICS`, `OPENGL_VERSION`, `CONTEXT`). Requires **CMake 3.22+**.

```bash
cmake -S . -B build -G Ninja \
  -DPYROS_GRAPHICS=OpenGL \
  -DOPENGL_VERSION=GL41 \
  -DBUILD_DEMOS=ON
cmake --build build -j
```

## Windows (MSVC)

OpenGL and Vulkan, DLL or static. Dependencies come from
[vcpkg](https://vcpkg.io); Vulkan additionally needs the LunarG SDK, which
also supplies the shaderc + SPIRV-Cross that `BUILD_SPIRV_TOOLING` looks for.

`vcpkg.json` at the repo root declares the dependency set, so the toolchain
installs them for you — no separate `vcpkg install` needed. It pins Lua to
5.4.x on purpose: the current port is 5.5, which the vendored `sol.hpp` does
not support.

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DPYROS_GRAPHICS=Vulkan -DCONTEXT=SDL2Vulkan `
  -DBUILD_DEMOS=ON -DBUILD_EDITOR=ON
cmake --build build --parallel
```

Keep the installed Vulkan SDK at or above the `volk` tag pinned in
`cmake/PyrosBackend.cmake` — volk references entry points declared in the
Vulkan headers of its own release.

`PYROS_ASSET_ROOT` controls where DemoLauncher and PyrosBuilder look for
their Lua assets. It defaults to the absolute source path, which is right
for a local build; pass `-DPYROS_RELOCATABLE_ASSETS=ON` for a redistributable
one and ship `assets/` and `shaders/` next to the executable.

Prebuilt artifacts for all four combinations are produced by
[`.github/workflows/windows.yml`](.github/workflows/windows.yml) on every push.

## Emscripten / WebGL2

Use **emcmake** (GLES3 + SDL2 + **Box3D**). Init submodules first. The old Premake/GLES2 tree was removed.

See [otherplatforms/emscripten/README.md](otherplatforms/emscripten/README.md).

```bash
git submodule update --init --recursive
emcmake cmake -S . -B build_web -G "Unix Makefiles" \
  -DBUILD_DEMOS=ON
cmake --build build_web -j --target Pyros3D
# serve build_web/examples/ and open web/index.html (JS owns the engine)
```

Physics from JS: `new P.Box3DPhysics()` (Embind). DemoLauncher “Simple Physics” on native uses the same `Physics` → `Box3DPhysics` path.

## Raspberry Pi (GLES3)

Same OpenGL ES 3.0 profile as WebGL2, via Mesa + SDL2 (on-device CMake).  
See [otherplatforms/raspberry-pi/README.md](otherplatforms/raspberry-pi/README.md).

```bash
cmake -S . -B build_rpi -G Ninja \
  -DOPENGL_VERSION=GLES3 -DCONTEXT=SDL2 \
  -DBUILD_VULKAN_BACKEND=OFF -DBUILD_SPIRV_TOOLING=OFF \
  -DBUILD_DEMOS=ON -DSTATIC_LIB=ON
cmake --build build_rpi -j
```

## Android (GLES3 + SDL2)

NDK CMake scaffolding (engine static lib). **Not tested on device yet.** Legacy ndk-build / GLES2 removed.  
See [otherplatforms/android/README.md](otherplatforms/android/README.md).

## Running Example
[Legacy WebGL JS demo](https://www.duartepeixinho.com/pyrosjs/) — new path is the emcmake `Pyros3D` library + `examples/web/`.

## Screenshots
![Rotating Cube](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/RotatingCube/Rotating%20Cube.png)
![Rotating Textured Cube](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/RotatingTexturedCube/RotatingTexturedCube.png)
![Deferred Rendering](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/DeferredRendering/DeferredRendering.png)
![Island Demo](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/IslandDemo/IslandDemo.png)
![Shadows](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/RotatingCubeWithLightingAndShadow/Rotating%20Cube%20With%20Lighting%20And%20Shadows.png)
![Physics](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/SimplePhysics/Simple%20Physics%20Example.png)
![Skeleton Animation](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/SkeletonAnimationExample/SkeletonAnimation.png)
![Text Rendering](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/TextRendering/TextRendering.png)
![Decals](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/Decals/Decals.png)
![VR](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/otherplatforms/vr/VR_ShootingRange/VR_ShootingRange.png)

License
----

MIT
