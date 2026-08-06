# Pyros3D Game Engine
[![Build](https://github.com/Peixinho/Pyros3D/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/Peixinho/Pyros3D/actions/workflows/build.yml)

Pyros3D is a work in progress game engine, focused on 3D, but that has 2 projects launched on android in 2D.
- [Bang! Bang! Bunny](https://play.google.com/store/apps/details?id=com.madblowfish.bangbangbunny)
- [Bang! Bang! Judas](https://play.google.com/store/apps/details?id=com.madblowfish.bangbangjudas)

You can compile for Windows, Linux, MacOSX, Android, and Web (Emscripten / WebGL2).

## Dependencies
- BulletPhysics 2.8+
- Freetype 2.6+
- Assimp 3.0 (to build tools/AssimImporter that converts regular 3D models (obj,dae, ...) to pyros format)
- SDL2 (examples / DemoLauncher)
- Lua 5.1–5.4 (optional, for Lua bindings / DemoLauncher)

To build natively, run CMake (see also `cmake/PyrosOptions.cmake` for `PYROS_GRAPHICS`, `OPENGL_VERSION`, `CONTEXT`).

```bash
cmake -S . -B build -G Ninja \
  -DPYROS_GRAPHICS=OpenGL \
  -DOPENGL_VERSION=GL41 \
  -DBUILD_DEMOS=ON
cmake --build build -j
```

## Emscripten / WebGL2

Use **emcmake** (GLES3 + SDL2). The old Premake/GLES2 tree was removed.

See [otherplatforms/emscripten/README.md](otherplatforms/emscripten/README.md).

```bash
emcmake cmake -S . -B build_web -G "Unix Makefiles" \
  -DBUILD_DEMOS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build_web -j --target Pyros3D
# serve build_web/examples/ and open web/index.html (JS owns the engine)
```

## Running Example
[Legacy WebGL JS demo](https://www.duartepeixinho.com/pyrosjs/) — new path is the emcmake `Pyros3D` library + `examples/web/`.

## Screenshots
![Rotating Cube](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/RotatingCube/Rotating%20Cube.png)
![Rotating Textured Cube](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/RotatingTexturedCube/RotatingTexturedCube.png)
![Deferred Rendering](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/DeferredRendering/DeferredRendering.png)
![Island Demo](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/IslandDemo/IslandDemo.png)
![Picking](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/PickingPainterMethod/Picking%20With%20Painter%20Method.png)
![Shadows](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/RotatingCubeWithLightingAndShadow/Rotating%20Cube%20With%20Lighting%20And%20Shadows.png)
![Physics](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/SimplePhysics/Simple%20Physics%20Example.png)
![Skeleton Animation](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/SkeletonAnimationExample/SkeletonAnimation.png)
![Text Rendering](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/TextRendering/TextRendering.png)
![Decals](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/examples/Decals/Decals.png)
![VR](https://raw.githubusercontent.com/Peixinho/Pyros3D/master/otherplatforms/vr/VR_ShootingRange/VR_ShootingRange.png)

License
----

MIT
