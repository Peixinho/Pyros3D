# Pyros3D Game Engine

Pyros3D is a work in progress game engine, focused on 3D, but that has 2 projects launched on android in 2D.
You can compile for Windows, Linux, MacOSX, Android, and Javascript (ios should work almost out of the box but never tried)

To build it just run premake. Here are examples to create builds for x64 and examples.

## Dependencies
- SFML 2.3 (better supported) / SDL2 / SDL1.2
- BulletPhysics 2.8
- Freetype 2.6

## Windows:
```sh
premake4.exe --examples --x64 vs2015
```

## Linux / Mac OSX:
```sh
./premake4 --examples --x64 gmake
```

## Android / Emscripten
There are specific readme in android section.

## Running Example
[Working WebGL Demo](http://duartepeixinho.com/pyrosjs/)

License
----

MIT