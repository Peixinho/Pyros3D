# Raspberry Pi (Vulkan)

Build on the Pi with **Vulkan + SDL2**, through Mesa's Vulkan driver for the Pi's GPU (v3dv). Use modern
**Pi OS (Bookworm+) Mesa** drivers — do **not** use the legacy Broadcom `/opt/vc` stack.

Not yet run on a Pi since the renderer moved to Vulkan / Metal / WebGPU: treat these steps as the starting point,
and expect the renderer's full picture (volumetric clouds, AO, reflections, TAA) to be heavy for the Pi's GPU -
a scene's Scene Effects can turn them off.

## Requirements

- Raspberry Pi **4 or 5** (v3dv exposes Vulkan 1.2 on both; the Pi 3 has no Vulkan driver)
- Pi OS 64-bit Bookworm (or similar) with desktop / KMS
- Packages:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build git \
  libsdl2-dev \
  libfreetype-dev \
  liblua5.4-dev \
  libvulkan-dev mesa-vulkan-drivers vulkan-tools \
  glslc glslang-dev
```

Confirm Vulkan is available:

```bash
vulkaninfo --summary    # should list the V3D device
```

## Configure & build (on-device)

From the repo root:

```bash
cmake -S . -B build_rpi -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPYROS_GRAPHICS=Vulkan \
  -DBUILD_DEMOS=ON \
  -DSTATIC_LIB=ON

cmake --build build_rpi -j"$(nproc)"
```

`glslc` compiles the renderer's shaders at build time (they are embedded in the engine); glslang compiles
materials' surfaces at run time and is optional. volk and VMA come through FetchContent, so the first configure
needs network.

Run from the examples build dir:

```bash
cd build_rpi/examples
./DemoLauncher        # PYROS_DEMO_INDEX=<n> picks a demo; PYROS_SHOT=/tmp/f.png saves frame 120
```

## Notes

- The window is SDL2's; the Vulkan surface is the X11 / Wayland window SDL makes.
- Cross-compile from x86 is possible with an aarch64 toolchain + sysroot, but
  on-device builds are the supported first step.
