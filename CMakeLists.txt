cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(Pyros3D)
set(CMAKE_CXX_STANDARD 17)

# An empty build type means no -O flag at all. The day-to-day editor build
# dirs were configured that way and ran ~4x slower on the CPU, and build_game
# shipped the -O0 PyrosPlayer sitting next to them. Pass
# -DCMAKE_BUILD_TYPE=Debug when a debug build is actually wanted.
get_property(PYROS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT PYROS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
	message(STATUS "CMAKE_BUILD_TYPE not set - defaulting to RelWithDebInfo")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Windows/MSVC-wide defines and flags. Must come before anything that adds a
# target (box3d via PyrosDependencies, the vendored Ext sources) - a
# no-op everywhere else.
include(PyrosWindows)

# pyros_stage_dir() - shaders/assets next to the built binaries. Defined at
# root scope so examples/ and editor/ both see it.
include(PyrosStageAssets)

# Options → dependencies → devices/ImGui → engine sources
include(PyrosOptions)
include(PyrosDependencies)
include(PyrosBackend)
include(PyrosSources)

# ---------------------------------------------------------------------------
# PyrosEngine library
# ---------------------------------------------------------------------------
add_library(PyrosEngine ${LIB_TYPE} ${PYROS_ENGINE_SOURCE})
include(PyrosRenderer)
pyros_add_renderer(PyrosEngine)
# (miniaudio on iOS includes AVFoundation - Objective-C: its implementation is compiled as Objective-C++ there)
if (IOS)
	set_source_files_properties(${CMAKE_SOURCE_DIR}/src/Pyros3D/Ext/miniaudio/miniaudio_impl.cpp PROPERTIES LANGUAGE OBJCXX)
endif()

target_include_directories(PyrosEngine
	PUBLIC
		${CMAKE_SOURCE_DIR}/include
		${IMGUI_INCLUDE_DIRS}
		${LUA_INCLUDE_DIR}
		${FREETYPE_INCLUDE_DIRS}
		${BOX3D_INCLUDE_DIRS}
		${BOX2D_INCLUDE_DIRS}
	PRIVATE
		${PYROS_DEVICE_INCLUDE_DIRS}
		${ENET_INCLUDE_DIRS}
)

target_link_libraries(PyrosEngine
	PUBLIC
		${BOX3D_LIBRARIES}
		${BOX2D_LIBRARIES}
		${FREETYPE_LIBRARIES}
		${LUA_LIBRARIES}
		${VULKAN_BACKEND_LIBS}
		${METAL_BACKEND_LIBS}
		${AUDIO_LIBS}
)

# Networking: ENet stays private - nothing in include/Pyros3D names it - and
# PYROS_NETWORKING tells the engine and its users the transport is there.
target_link_libraries(PyrosEngine PRIVATE ${MONOCYPHER_LIBRARIES})
# (Apple: AppKit/UIKit for the device's layer and window)
if (APPLE AND NOT EMSCRIPTEN)
	if (IOS)
		target_link_libraries(PyrosEngine PRIVATE "-framework UIKit")
	else()
		target_link_libraries(PyrosEngine PRIVATE "-framework AppKit")
	endif()
endif()
target_link_libraries(PyrosEngine PRIVATE ${RECAST_LIBRARIES})
if (ENET_LIBRARIES)
	target_link_libraries(PyrosEngine PRIVATE ${ENET_LIBRARIES})
	target_compile_definitions(PyrosEngine PUBLIC PYROS_NETWORKING)
endif()

# Android File.cpp uses SDL_RWops — link SDL2 into the engine itself.
if (ANDROID)
	target_link_libraries(PyrosEngine PUBLIC SDL2::SDL2)
endif()


# echo() is a macro around an inline function in Log.h, so the define has to
# reach every translation unit that calls it, not just the engine's own -
# hence PUBLIC rather than PRIVATE.
if (PYROS_LOG STREQUAL "Console")
	target_compile_definitions(PyrosEngine PUBLIC LOG_TO_CONSOLE)
elseif (PYROS_LOG STREQUAL "File")
	target_compile_definitions(PyrosEngine PUBLIC LOG_TO_FILE)
else()
	target_compile_definitions(PyrosEngine PUBLIC LOG_DISABLE)
endif()

# PYROS3D_API (Export.h): dllexport when building the shared DLL on Windows.
if (WIN32 AND LIB_TYPE STREQUAL "SHARED")
	target_compile_definitions(PyrosEngine PRIVATE _EXPORT)

	# ImGui is compiled into the engine but carries no PYROS3D_API - it has
	# an export macro of its own instead. The demos and the editor call it
	# directly, so without this a SHARED build fails with LNK2001/LNK2019 on
	# every ImGui:: symbol.
	#
	# PRIVATE vs INTERFACE is what splits export from import: PRIVATE reaches
	# only the engine's own translation units, INTERFACE only what links it.
	target_compile_definitions(PyrosEngine
		PRIVATE   "IMGUI_API=__declspec(dllexport)"
		INTERFACE "IMGUI_API=__declspec(dllimport)"

		# The ImGui SDL backend is built by each consumer (see
		# PyrosWindowContext.cmake) and called on the same side of the DLL, so
		# its entry points stay plain - IMGUI_IMPL_API defaults to IMGUI_API,
		# and leaving that to become dllimport in the consumer would reject the
		# definitions the consumer compiles itself (C2491).
		PUBLIC    "IMGUI_IMPL_API="
	)
endif()

if (EMSCRIPTEN)
	pyros_emscripten_apply_compile(PyrosEngine)
endif()

# ---------------------------------------------------------------------------
# Optional tools / demos
# ---------------------------------------------------------------------------
if (BUILD_CONVERTER)
	add_subdirectory(${CMAKE_SOURCE_DIR}/tools/AssimpImporter)
endif()

if (BUILD_DEMOS)
	add_subdirectory(${CMAKE_SOURCE_DIR}/examples)
	# (the visual regression check over the demos - macOS: it captures windows with CoreGraphics)
	if (APPLE AND NOT EMSCRIPTEN AND NOT IOS)
		add_subdirectory(${CMAKE_SOURCE_DIR}/tools/VisualCheck)
	endif()
endif()

if (BUILD_EDITOR)
	add_subdirectory(${CMAKE_SOURCE_DIR}/editor)
endif()

if (BUILD_PLAYER)
	# On the web too: player/ builds PyrosPlayer.js/.wasm and the page that
	# loads a built game into it (player/web). A game's own files are not
	# baked into the module - Build Game packs them beside it.
	add_subdirectory(${CMAKE_SOURCE_DIR}/player)
endif()
