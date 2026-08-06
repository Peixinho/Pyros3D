# User-facing build options (cmake-gui / ccmake dropdowns via STRINGS).
# Included from the root CMakeLists.txt.

# ---------------------------------------------------------------------------
# Primary graphics stack
# ---------------------------------------------------------------------------
set(PYROS_GRAPHICS "OpenGL" CACHE STRING "Primary graphics backend")
set_property(CACHE PYROS_GRAPHICS PROPERTY STRINGS OpenGL Vulkan)

# ---------------------------------------------------------------------------
# OpenGL / GLES profile (glad loader). Still compiled into the engine even
# for Vulkan builds (GLRenderDevice remains available); the window path is
# chosen separately via CONTEXT.
# ---------------------------------------------------------------------------
set(OPENGL_VERSION "GL41" CACHE STRING "OpenGL/GLES profile for glad + shader #version")
set_property(CACHE OPENGL_VERSION PROPERTY STRINGS GL45 GL42 GL41 GLES3)

# ---------------------------------------------------------------------------
# Window / swapchain context
#   Auto       → SDL2 for OpenGL, SDL2Vulkan for Vulkan
#   SDL2       → OpenGL window via SDL2
#   SDL2Vulkan → Vulkan window via SDL2 (requires Vulkan backend)
#   SFML / SDL → legacy contexts
# ---------------------------------------------------------------------------
set(CONTEXT "Auto" CACHE STRING "Window context (Auto picks SDL2 or SDL2Vulkan from PYROS_GRAPHICS)")
set_property(CACHE CONTEXT PROPERTY STRINGS Auto SDL2 SDL2Vulkan SFML SDL)

if (CONTEXT STREQUAL "Auto" OR CONTEXT STREQUAL "")
	if (PYROS_GRAPHICS STREQUAL "Vulkan")
		set(PYROS_CONTEXT "SDL2Vulkan")
	else()
		set(PYROS_CONTEXT "SDL2")
	endif()
else()
	set(PYROS_CONTEXT "${CONTEXT}")
endif()

# ---------------------------------------------------------------------------
# Feature toggles
# ---------------------------------------------------------------------------
option(IS_DESKTOP "Desktop target (vs embedded)" ON)
option(HAVE_LUA_BINDINGS "Build Lua/sol bindings" ON)
option(STATIC_LIB "Build PyrosEngine as a static library" OFF)
option(BUILD_SPIRV_TOOLING "GLSL→SPIR-V tooling (shaderc + spirv-cross)" ON)
option(BUILD_CONVERTER "Build Assimp model converter tool" OFF)
option(BUILD_DEMOS "Build DemoLauncher / CppApiDemo" OFF)

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
message(STATUS "  BUILD_SPIRV_TOOLING  = ${BUILD_SPIRV_TOOLING}")
message(STATUS "  BUILD_DEMOS          = ${BUILD_DEMOS}")
message(STATUS "  HAVE_LUA_BINDINGS    = ${HAVE_LUA_BINDINGS}")
message(STATUS "  LIB_TYPE             = ${LIB_TYPE}")
