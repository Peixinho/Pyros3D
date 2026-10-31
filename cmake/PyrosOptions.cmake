# User-facing build options (cmake-gui / ccmake dropdowns via STRINGS).
# Included from the root CMakeLists.txt.

# ---------------------------------------------------------------------------
# pyros_frameworks_by_name(<var>)
#
# Rewrites every /any/path/Foo.framework in <var> to "-framework Foo", in
# place, leaving everything else alone.
#
# An absolute .framework on a link line makes CMake emit `-F<that SDK's
# Frameworks dir>` for the target, and that search path then decides where
# EVERY framework on the line is resolved from - CoreAudio, CoreFoundation
# and the rest, not just the one that was found by path. On a machine where
# the SDK find_package picked and the linker xcode-select picked come from
# different installs, the link dies on frameworks the build never asked
# about:
#
#   OpenGL.tbd:4:20: error: unknown architecture arm64e.x1-macos
#   ... tapi error: malformed file
#
# By name, the compiler driver resolves them from its own sysroot, which is
# by construction the one its linker can read. The find_package/find_library
# calls still run - they are what answer "is this framework here at all".
# ---------------------------------------------------------------------------
function(pyros_frameworks_by_name var)
	if (NOT APPLE OR EMSCRIPTEN)
		return()
	endif()
	set(_out "")
	foreach (_lib IN LISTS ${var})
		if (_lib MATCHES "/([A-Za-z0-9_]+)\\.framework/?$")
			list(APPEND _out "-framework ${CMAKE_MATCH_1}")
		else()
			list(APPEND _out "${_lib}")
		endif()
	endforeach()
	if (_out)
		list(REMOVE_DUPLICATES _out)
	endif()
	set(${var} ${_out} PARENT_SCOPE)
endfunction()

# Emscripten (emcmake) and Android NDK force GLES3 / SDL2 / no Vulkan first.
include(${CMAKE_CURRENT_LIST_DIR}/PyrosEmscripten.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/PyrosAndroid.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/PyrosIOS.cmake)

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
# The GPU API the renderer draws with (its device: include/Pyros3D/Rendering/Device)
#   Metal  - Apple (the default there)
#   Vulkan - Linux, Windows (the default there); a Mac through MoltenVK
#   WebGPU - the browser (forced under Emscripten); natively through Dawn
# The window is SDL2's, whichever it is (examples/WindowManagers/SDL2).
# ---------------------------------------------------------------------------
if (EMSCRIPTEN)
	set(_pyros_graphics_default WebGPU)
elseif (APPLE)
	set(_pyros_graphics_default Metal)
else()
	set(_pyros_graphics_default Vulkan)
endif()
set(PYROS_GRAPHICS "${_pyros_graphics_default}" CACHE STRING "The GPU API the renderer draws with")
set_property(CACHE PYROS_GRAPHICS PROPERTY STRINGS Metal Vulkan WebGPU)
# (a build dir configured before OpenGL went: what it would have drawn with now)
if (PYROS_GRAPHICS STREQUAL "OpenGL")
	set(PYROS_GRAPHICS "${_pyros_graphics_default}" CACHE STRING "The GPU API the renderer draws with" FORCE)
endif()
unset(_pyros_graphics_default)

if (PYROS_GRAPHICS STREQUAL "Metal" AND NOT APPLE)
	message(FATAL_ERROR "PYROS_GRAPHICS=Metal is only available on Apple platforms (use Vulkan on Windows/Linux)")
endif()
set(PYROS_CONTEXT "SDL2")

# ---------------------------------------------------------------------------
# Feature toggles
# ---------------------------------------------------------------------------
option(IS_DESKTOP "Desktop target (vs embedded)" ON)
option(HAVE_LUA_BINDINGS "Build Lua/sol bindings" ON)
option(STATIC_LIB "Build PyrosEngine as a static library" OFF)
option(BUILD_CONVERTER "Build Assimp model converter tool" OFF)
option(BUILD_DEMOS "Build DemoLauncher" OFF)
option(BUILD_EDITOR "Build the PyrosBuilder scene editor" OFF)
# The runtime a built game ships with. Defaults ON alongside the editor,
# because the editor's Build Game needs a player binary to copy - an editor
# that can export a game only to report there is nothing to export with is
# the wrong default.
option(BUILD_PLAYER "Build the PyrosPlayer standalone game runtime" OFF)
if (BUILD_EDITOR AND NOT BUILD_PLAYER)
	set(BUILD_PLAYER ON CACHE BOOL "Build the PyrosPlayer standalone game runtime" FORCE)
endif()
# Scripting is not optional for a player: a game's behaviour lives in Lua.
if (BUILD_PLAYER AND NOT HAVE_LUA_BINDINGS)
	message(STATUS "BUILD_PLAYER requested without Lua bindings - the player would run no scripts, skipping")
	set(BUILD_PLAYER OFF CACHE BOOL "Build the PyrosPlayer standalone game runtime" FORCE)
endif()

# ---------------------------------------------------------------------------
# Asset root baked into DemoLauncher (EXAMPLES_PATH) and PyrosBuilder
# (PYROS_EXAMPLES_PATH) - middleclass.lua, the demo manifest and the demo
# scenes are all looked up under it.
#
# Absolute source path by default, so a local build finds them wherever the
# working directory happens to be. PYROS_RELOCATABLE_ASSETS=ON switches them
# to "." for a redistributable build (CI artifacts): the lookups then resolve
# relative to the working directory and the assets ship next to the binary.
# ---------------------------------------------------------------------------
# Prefer the boolean over passing -DPYROS_ASSET_ROOT=. by hand: a bare "."
# does not survive PowerShell's native-argument parsing, which silently
# produced -DEXAMPLES_PATH="" and a run of baffling preprocessor errors in
# DemoLauncher rather than anything pointing at the real cause.
option(PYROS_RELOCATABLE_ASSETS
	"Resolve demo/editor assets relative to the working directory, for a redistributable build" OFF)

if (PYROS_RELOCATABLE_ASSETS)
	set(_pyros_asset_root_default ".")
else()
	set(_pyros_asset_root_default "${CMAKE_SOURCE_DIR}/examples")
endif()

set(PYROS_ASSET_ROOT "${_pyros_asset_root_default}" CACHE STRING
	"Root the demo/editor Lua assets are loaded from (see PYROS_RELOCATABLE_ASSETS)")
unset(_pyros_asset_root_default)

# EXAMPLES_PATH/PYROS_EXAMPLES_PATH are stringified by the preprocessor
# (STR()/STR_EX()), so an empty value expands to nothing at all rather than
# to "" - turning `lua["X"] = STR(EXAMPLES_PATH);` into `lua["X"] = ;`. Fail
# here, where the cause is legible, instead of in the middle of the build.
if (PYROS_ASSET_ROOT STREQUAL "")
	message(FATAL_ERROR
		"PYROS_ASSET_ROOT is empty. Use -DPYROS_RELOCATABLE_ASSETS=ON for a "
		"relocatable build, or give it a real path.")
endif()

# Prefer enabling the converter with the editor when Assimp is available.
if (BUILD_EDITOR AND NOT BUILD_CONVERTER)
	find_package(assimp QUIET)
	if (assimp_FOUND OR ASSIMP_FOUND)
		set(BUILD_CONVERTER ON CACHE BOOL "Build Assimp model converter tool" FORCE)
		message(STATUS "BUILD_EDITOR + Assimp found → enabling BUILD_CONVERTER")
	else()
		message(STATUS "Assimp not found — Import Model will still accept .p3dm; source conversion needs AssimpImporter")
	endif()
endif()

# The devices built: the one PYROS_GRAPHICS names, and (overridable) Metal beside Vulkan on a Mac, so either
# draws (PYROS_GFX_BACKEND picks at run time - Device.h)
if (PYROS_GRAPHICS STREQUAL "Vulkan")
	set(_pyros_vulkan_default ON)
else()
	set(_pyros_vulkan_default OFF)
endif()
option(BUILD_VULKAN_BACKEND "Build the Vulkan device (Vulkan SDK + volk + VMA)" ${_pyros_vulkan_default})
if (PYROS_GRAPHICS STREQUAL "Metal")
	set(_pyros_metal_default ON)
else()
	set(_pyros_metal_default OFF)
endif()
option(BUILD_METAL_BACKEND "Build the Metal device (Metal.framework + spirv-cross, Apple only)" ${_pyros_metal_default})
unset(_pyros_vulkan_default)
unset(_pyros_metal_default)

if (PYROS_GRAPHICS STREQUAL "Vulkan" AND NOT BUILD_VULKAN_BACKEND)
	message(FATAL_ERROR "PYROS_GRAPHICS=Vulkan requires BUILD_VULKAN_BACKEND=ON")
endif()
if (PYROS_GRAPHICS STREQUAL "Metal" AND NOT BUILD_METAL_BACKEND)
	message(FATAL_ERROR "PYROS_GRAPHICS=Metal requires BUILD_METAL_BACKEND=ON")
endif()
if (BUILD_METAL_BACKEND AND NOT APPLE)
	message(FATAL_ERROR "BUILD_METAL_BACKEND is only available on Apple platforms")
endif()
if (EMSCRIPTEN AND (BUILD_VULKAN_BACKEND OR BUILD_METAL_BACKEND))
	message(FATAL_ERROR "The browser build draws with WebGPU only")
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
message(STATUS "  PYROS_GRAPHICS       = ${PYROS_GRAPHICS}")
message(STATUS "  BUILD_VULKAN_BACKEND = ${BUILD_VULKAN_BACKEND}")
message(STATUS "  BUILD_METAL_BACKEND  = ${BUILD_METAL_BACKEND}")
message(STATUS "  BUILD_DEMOS          = ${BUILD_DEMOS}")
message(STATUS "  BUILD_EDITOR         = ${BUILD_EDITOR}")
message(STATUS "  BUILD_PLAYER         = ${BUILD_PLAYER}")
message(STATUS "  HAVE_LUA_BINDINGS    = ${HAVE_LUA_BINDINGS}")
message(STATUS "  LIB_TYPE             = ${LIB_TYPE}")
message(STATUS "  PYROS_ASSET_ROOT     = ${PYROS_ASSET_ROOT}")
if (EMSCRIPTEN)
	message(STATUS "  EMSCRIPTEN           = ON (WebGPU)")
endif()
if (ANDROID)
	message(STATUS "  ANDROID              = ON (Vulkan + SDL2 - untested; see otherplatforms/android/README.md)")
endif()
