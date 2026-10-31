# Applied when configuring with the Android NDK CMake toolchain
# (CMAKE_SYSTEM_NAME=Android / ANDROID=1). Untested on device - scaffolding
# for a Vulkan + SDL2 build; replace legacy ndk-build.

if (NOT ANDROID)
	return()
endif()

message(STATUS "Pyros3D: Android NDK detected - targeting Vulkan + SDL2 (untested)")

# ---------------------------------------------------------------------------
# Force a mobile-sane option set
# ---------------------------------------------------------------------------
set(PYROS_GRAPHICS "Vulkan" CACHE STRING "The GPU API the renderer draws with" FORCE)

set(STATIC_LIB ON CACHE BOOL "Build PyrosEngine as a static library" FORCE)
set(LIB_TYPE STATIC)

set(BUILD_VULKAN_BACKEND ON CACHE BOOL "Build the Vulkan device" FORCE)
set(BUILD_CONVERTER OFF CACHE BOOL "Build Assimp model converter tool" FORCE)
# No Gradle activity shell yet — build the engine lib only.
set(BUILD_DEMOS OFF CACHE BOOL "Build DemoLauncher" FORCE)
set(HAVE_LUA_BINDINGS OFF CACHE BOOL "Build Lua/sol bindings" FORCE)

set(IS_DESKTOP ON CACHE BOOL "Desktop target (vs embedded)" FORCE)

add_compile_definitions(ANDROID _ANDROID LOG_TO_CONSOLE)
