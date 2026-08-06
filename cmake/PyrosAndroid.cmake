# Applied when configuring with the Android NDK CMake toolchain
# (CMAKE_SYSTEM_NAME=Android / ANDROID=1). Untested on device — scaffolding
# aligned with the GLES3 + SDL2 web/RPi path; replace legacy ndk-build.

if (NOT ANDROID)
	return()
endif()

message(STATUS "Pyros3D: Android NDK detected — targeting OpenGL ES 3 + SDL2 (untested)")

# ---------------------------------------------------------------------------
# Force a mobile-sane option set (same GLES3 profile as WebGL2 / RPi)
# ---------------------------------------------------------------------------
set(PYROS_GRAPHICS "OpenGL" CACHE STRING "Primary graphics backend" FORCE)
set(OPENGL_VERSION "GLES3" CACHE STRING "OpenGL/GLES profile for glad + shader #version" FORCE)
set(CONTEXT "SDL2" CACHE STRING "Window context" FORCE)
set(PYROS_CONTEXT "SDL2")

set(STATIC_LIB ON CACHE BOOL "Build PyrosEngine as a static library" FORCE)
set(LIB_TYPE STATIC)

set(BUILD_VULKAN_BACKEND OFF CACHE BOOL "Build VulkanRenderDevice" FORCE)
set(BUILD_SPIRV_TOOLING OFF CACHE BOOL "GLSL→SPIR-V tooling" FORCE)
set(BUILD_CONVERTER OFF CACHE BOOL "Build Assimp model converter tool" FORCE)
# No Gradle activity shell yet — build the engine lib only.
set(BUILD_DEMOS OFF CACHE BOOL "Build DemoLauncher / CppApiDemo" FORCE)
set(HAVE_LUA_BINDINGS OFF CACHE BOOL "Build Lua/sol bindings" FORCE)

# glad GLES3 loader (same as Emscripten / RPi Mesa path)
set(IS_DESKTOP ON CACHE BOOL "Desktop target (vs embedded)" FORCE)

add_compile_definitions(ANDROID _ANDROID LOG_TO_CONSOLE)
