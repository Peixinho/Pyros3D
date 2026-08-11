# User-facing build options (cmake-gui / ccmake dropdowns via STRINGS).
# Included from the root CMakeLists.txt.

# Emscripten (emcmake) and Android NDK force GLES3 / SDL2 / no Vulkan first.
include(${CMAKE_CURRENT_LIST_DIR}/PyrosEmscripten.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/PyrosAndroid.cmake)

# ---------------------------------------------------------------------------
# Logging
#
# echo() is the engine's only error-reporting channel - 42 call sites, most
# of them echo("ERROR: ..."), including the one that reports a Lua component
# script raising. It expands to a body wrapped in
# #if defined(LOG_TO_FILE)/LOG_DISABLE/LOG_TO_CONSOLE/_DEBUG, and nothing
# defined any of them, so every one of those messages was discarded. A
# script failing on an unbound binding therefore looked exactly like a
# renderer that drew nothing, which is a genuinely expensive way to debug.
# Console by default: silence should be opted into, not the default.
# ---------------------------------------------------------------------------
set(PYROS_LOG "Console" CACHE STRING "Where echo() output goes")
set_property(CACHE PYROS_LOG PROPERTY STRINGS Console File Off)

# ---------------------------------------------------------------------------
# Primary graphics stack
# ---------------------------------------------------------------------------
set(PYROS_GRAPHICS "OpenGL" CACHE STRING "Primary graphics backend")
set_property(CACHE PYROS_GRAPHICS PROPERTY STRINGS OpenGL Vulkan Metal)

if (PYROS_GRAPHICS STREQUAL "Metal" AND NOT APPLE)
	message(FATAL_ERROR "PYROS_GRAPHICS=Metal is only available on Apple platforms (use Vulkan on Windows/Linux)")
endif()

# ---------------------------------------------------------------------------
# OpenGL / GLES profile (glad loader). Still compiled into the engine even
# for Vulkan builds (GLRenderDevice remains available); the window path is
# chosen separately via CONTEXT.
# ---------------------------------------------------------------------------
set(OPENGL_VERSION "GL41" CACHE STRING "OpenGL/GLES profile for glad + shader #version")
set_property(CACHE OPENGL_VERSION PROPERTY STRINGS GL45 GL42 GL41 GLES3)

# ---------------------------------------------------------------------------
# Window / swapchain context
#   Auto       → SDL2 for OpenGL, SDL2Vulkan for Vulkan, SDL2Metal for Metal
#   SDL2       → OpenGL window via SDL2
#   SDL2Vulkan → Vulkan window via SDL2 (requires Vulkan backend)
#   SDL2Metal  → native Metal window via SDL2 (requires Metal backend, Apple only)
#   SFML / SDL → legacy contexts
# ---------------------------------------------------------------------------
set(CONTEXT "Auto" CACHE STRING "Window context (Auto picks SDL2/SDL2Vulkan/SDL2Metal from PYROS_GRAPHICS)")
set_property(CACHE CONTEXT PROPERTY STRINGS Auto SDL2 SDL2Vulkan SDL2Metal SFML SDL)

if (CONTEXT STREQUAL "Auto" OR CONTEXT STREQUAL "")
	if (PYROS_GRAPHICS STREQUAL "Vulkan")
		set(PYROS_CONTEXT "SDL2Vulkan")
	elseif (PYROS_GRAPHICS STREQUAL "Metal")
		set(PYROS_CONTEXT "SDL2Metal")
	else()
		set(PYROS_CONTEXT "SDL2")
	endif()
else()
	set(PYROS_CONTEXT "${CONTEXT}")
endif()

if (PYROS_CONTEXT STREQUAL "SDL2Metal" AND NOT APPLE)
	message(FATAL_ERROR "CONTEXT=SDL2Metal is only available on Apple platforms (use SDL2Vulkan on Windows/Linux)")
endif()

# ---------------------------------------------------------------------------
# Feature toggles
# ---------------------------------------------------------------------------
option(IS_DESKTOP "Desktop target (vs embedded)" ON)
# Lua defaults off under Emscripten (CppApiDemo first); on for native.
if (EMSCRIPTEN)
	set(_pyros_lua_default OFF)
else()
	set(_pyros_lua_default ON)
endif()
option(HAVE_LUA_BINDINGS "Build Lua/sol bindings" ${_pyros_lua_default})
option(STATIC_LIB "Build PyrosEngine as a static library" OFF)
option(BUILD_SPIRV_TOOLING "GLSL→SPIR-V tooling (shaderc + spirv-cross)" ON)
option(BUILD_CONVERTER "Build Assimp model converter tool" OFF)
option(BUILD_DEMOS "Build DemoLauncher / CppApiDemo" OFF)
option(BUILD_EDITOR "Build the PyrosBuilder scene editor (needs an OpenGL context)" OFF)

# Vulkan render device: on when the user picked Vulkan graphics or an
# explicit SDL2Vulkan context. Still overridable via -DBUILD_VULKAN_BACKEND=.
if (PYROS_GRAPHICS STREQUAL "Vulkan" OR PYROS_CONTEXT STREQUAL "SDL2Vulkan")
	set(_pyros_vulkan_default ON)
else()
	set(_pyros_vulkan_default OFF)
endif()
option(BUILD_VULKAN_BACKEND "Build VulkanRenderDevice (Vulkan SDK + volk + VMA)" ${_pyros_vulkan_default})

if (PYROS_CONTEXT STREQUAL "SDL2Vulkan" AND NOT BUILD_VULKAN_BACKEND)
	message(FATAL_ERROR
		"CONTEXT/PYROS_CONTEXT=SDL2Vulkan requires BUILD_VULKAN_BACKEND=ON "
		"(or set -DPYROS_GRAPHICS=Vulkan / -DBUILD_VULKAN_BACKEND=ON).")
endif()

if (EMSCRIPTEN AND BUILD_VULKAN_BACKEND)
	message(FATAL_ERROR "Vulkan backend is not supported under Emscripten (use GLES3 / WebGL2).")
endif()

# Metal render device: on when the user picked Metal graphics or an
# explicit SDL2Metal context. Still overridable via -DBUILD_METAL_BACKEND=.
# APPLE-only (guarded above already for PYROS_GRAPHICS/PYROS_CONTEXT
# directly - this covers the "neither was set to Metal but the user passed
# -DBUILD_METAL_BACKEND=ON by hand on Linux/Windows" case).
if (PYROS_GRAPHICS STREQUAL "Metal" OR PYROS_CONTEXT STREQUAL "SDL2Metal")
	set(_pyros_metal_default ON)
else()
	set(_pyros_metal_default OFF)
endif()
option(BUILD_METAL_BACKEND "Build MetalRenderDevice (Metal.framework + QuartzCore, Apple only)" ${_pyros_metal_default})

if (BUILD_METAL_BACKEND AND NOT APPLE)
	message(FATAL_ERROR "BUILD_METAL_BACKEND is only available on Apple platforms")
endif()

if (PYROS_CONTEXT STREQUAL "SDL2Metal" AND NOT BUILD_METAL_BACKEND)
	message(FATAL_ERROR
		"CONTEXT/PYROS_CONTEXT=SDL2Metal requires BUILD_METAL_BACKEND=ON "
		"(or set -DPYROS_GRAPHICS=Metal / -DBUILD_METAL_BACKEND=ON).")
endif()

if (EMSCRIPTEN AND BUILD_METAL_BACKEND)
	message(FATAL_ERROR "Metal backend is not supported under Emscripten (use GLES3 / WebGL2).")
endif()

# ---------------------------------------------------------------------------
# Library type
# ---------------------------------------------------------------------------
if (STATIC_LIB)
	set(LIB_TYPE STATIC)
else()
	set(LIB_TYPE SHARED)
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "Pyros3D options:")
message(STATUS "  PYROS_GRAPHICS     = ${PYROS_GRAPHICS}")
message(STATUS "  OPENGL_VERSION     = ${OPENGL_VERSION}")
message(STATUS "  CONTEXT (resolved) = ${PYROS_CONTEXT}  [cache=${CONTEXT}]")
message(STATUS "  BUILD_VULKAN_BACKEND = ${BUILD_VULKAN_BACKEND}")
message(STATUS "  BUILD_METAL_BACKEND  = ${BUILD_METAL_BACKEND}")
message(STATUS "  BUILD_SPIRV_TOOLING  = ${BUILD_SPIRV_TOOLING}")
message(STATUS "  BUILD_DEMOS          = ${BUILD_DEMOS}")
message(STATUS "  BUILD_EDITOR         = ${BUILD_EDITOR}")
message(STATUS "  HAVE_LUA_BINDINGS    = ${HAVE_LUA_BINDINGS}")
message(STATUS "  LIB_TYPE             = ${LIB_TYPE}")
if (EMSCRIPTEN)
	message(STATUS "  EMSCRIPTEN           = ON (WebGL2)")
endif()
if (ANDROID)
	message(STATUS "  ANDROID              = ON (GLES3 + SDL2 — untested; see otherplatforms/android/README.md)")
endif()
if (OPENGL_VERSION STREQUAL "GLES3" AND NOT EMSCRIPTEN AND NOT ANDROID)
	message(STATUS "  Native GLES3         = ON (Raspberry Pi / embedded — see otherplatforms/raspberry-pi/README.md)")
endif()
