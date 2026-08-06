# Raspberry Pi (OpenGL ES 3)

Build on the Pi with the **same GLES3 + SDL2 stack** as the Emscripten/WebGL2 path.
Use modern **Pi OS (Bookworm+) Mesa** drivers — do **not** use the legacy Broadcom
`/opt/vc` stack.

## Requirements

- Raspberry Pi **4 or 5** recommended (Pi 3 may work with Mesa GLES 3.0; untested)
- Pi OS 64-bit Bookworm (or similar) with desktop / KMS
- Packages:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build git \
  libsdl2-dev \
  libfreetype-dev \
  libbullet-dev \
  liblua5.3-dev \
  libgles2-mesa-dev \
  libegl1-mesa-dev
```

Confirm GLES is available:

```bash
glxinfo -B 2>/dev/null | head -20   # if mesa-utils installed
# or
es2_info 2>/dev/null
```

## Configure & build (on-device)

From the repo root:

```bash
cmake -S . -B build_rpi -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPYROS_GRAPHICS=OpenGL \
  -DOPENGL_VERSION=GLES3 \
  -DCONTEXT=SDL2 \
  -DIS_DESKTOP=ON \
  -DBUILD_VULKAN_BACKEND=OFF \
  -DBUILD_SPIRV_TOOLING=OFF \
  -DBUILD_DEMOS=ON \
  -DSTATIC_LIB=ON \
  -DLUA_INCLUDE_DIR=/usr/include/lua5.3 \
  -DLUA_LIBRARY=/usr/lib/aarch64-linux-gnu/liblua5.3.so

cmake --build build_rpi -j"$(nproc)"
```

`IS_DESKTOP=ON` keeps the glad GLES3 loader (same as Web). Leave Vulkan off —
the interesting path on Pi is GLES3 parity with the browser.

Run from the examples build dir (shaders are resolved relative to the binary / `EXAMPLES_PATH`):

```bash
cd build_rpi/examples
./CppApiDemo          # minimal ForwardRenderer cube — best first smoke test
# ./DemoLauncher      # only if HAVE_LUA_BINDINGS=ON
```

## Notes

- Context requests **OpenGL ES 3.0** (aligned with WebGL2).
- CMake links `libGLESv2` + `libEGL` via `find_library`.
- Cross-compile from x86 is possible with an aarch64 toolchain + sysroot, but
  on-device builds are the supported first step.
- Vulkan on Pi 4/5 is a later experiment; not wired here.
