# Pyros3D Game Engine

Pyros3D is a work in progress game engine, focused on 3D, but that has 2 projects launched on android in 2D.
- [Bang! Bang! Bunny](https://play.google.com/store/apps/details?id=com.madblowfish.bangbangbunny)
- [Bang! Bang! Judas](https://play.google.com/store/apps/details?id=com.madblowfish.bangbangjudas)

You can compile for Windows, Linux, MacOSX, Android, and Javascript (ios should work almost out of the box but never tried)

## Dependencies
- SFML 2.3 (better supported) / SDL2 / SDL1.2
- Glew
- BulletPhysics 2.8
- Freetype 2.6
- Assimp 3.0 (to build tools/AssimImporter that converts regular 3D models (obj,dae, ...) to pyros format)

To build it just run premake. Here are examples to create builds for x64 and examples.

## Windows:
```sh
premake4.exe --examples --x64 vs2015
```

## Linux / Mac OSX:
```sh
./premake4 --examples --x64 gmake
```

## Android / Emscripten
There are specific readme in android and emscripten sections.

## Running Example
[Working WebGL Demo](http://duartepeixinho.com/pyrosjs/)

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

License
----

MIT
