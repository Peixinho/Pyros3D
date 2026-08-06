# Android (OpenGL ES 3 + SDL2)

**Status: scaffolding only — not tested on a device yet.**

Same GLES3 + SDL2 profile as WebGL2 / Raspberry Pi. The old **ndk-build /
GLES2** tree (`jni/Android.mk`) was removed.

## What CMake does when `ANDROID=1`

Forced by `cmake/PyrosAndroid.cmake`:

| Option | Value |
|--------|--------|
| `OPENGL_VERSION` | `GLES3` |
| `CONTEXT` | `SDL2` |
| `BUILD_VULKAN_BACKEND` | `OFF` |
| `BUILD_DEMOS` | `OFF` (no Gradle activity yet) |
| `HAVE_LUA_BINDINGS` | `OFF` |
| `STATIC_LIB` | `ON` |

Also:

- Box3D via submodule `src/Pyros3D/Ext/box3d`; FreeType via FetchContent
- `File` uses `SDL_RWops` (`otherplatforms/android/src/File.cpp`)
- Links `GLESv2`, `EGL`, `android`, `log` (+ `GLESv3` / `OpenSLES` when present)

## Prerequisites

1. [Android NDK](https://developer.android.com/ndk) (r26+ recommended) with CMake
2. [SDL2](https://github.com/libsdl-org/SDL) built for Android (use SDL’s
   `android-project` / CMake Android docs) — you must pass include + library paths
3. Android Studio / Gradle app that loads your native lib (future work)

## Configure engine library (example)

```bash
# Adjust NDK path / ABI / API level for your machine.
export ANDROID_NDK=$HOME/Library/Android/sdk/ndk/26.1.10909125   # example

cmake -S . -B build_android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DCMAKE_BUILD_TYPE=Release \
  -DSDL2_INCLUDE_DIR=/path/to/SDL/include \
  -DSDL2_LIBRARY=/path/to/libSDL2.so

cmake --build build_android -j --target PyrosEngine
```

Output: `libPyrosEngine.a` (plus FetchContent deps). Wire it into an SDL2
Android activity the same way any native SDL game does — **that app shell is
not in this repo yet**.

## Assets

Package `resources/shaders` and any game assets under the APK’s
`assets/` so `SDL_RWFromFile("shaders/...")` / `"assets/..."` resolve.

## Vulkan

Not enabled. Prefer GLES3 first; Android Vulkan can follow later via
`SDL2Vulkan` once there is a tested GLES path.

## Legacy

Shipped games (Bang Bang Bunny / Judas) used an older GLES2 + ndk-build stack.
Do not revive `Android.mk`.
