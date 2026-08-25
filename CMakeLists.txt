cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(Pyros3D)
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Windows/MSVC-wide defines and flags. Must come before anything that adds a
# target (box3d via PyrosDependencies, the vendored Ext sources) - a
# no-op everywhere else.
include(PyrosWindows)

# pyros_stage_dir() - shaders/assets next to the built binaries. Defined at
# root scope so examples/ and editor/ both see it.
include(PyrosStageAssets)

# Options → dependencies → GL/Vulkan/ImGui → engine sources
include(PyrosOptions)
include(PyrosDependencies)
include(PyrosBackend)
include(PyrosSources)

# ---------------------------------------------------------------------------
# PyrosEngine library
# ---------------------------------------------------------------------------
add_library(PyrosEngine ${LIB_TYPE} ${PYROS_ENGINE_SOURCE})

target_include_directories(PyrosEngine
	PUBLIC
		${CMAKE_SOURCE_DIR}/include
		${IMGUI_INCLUDE_DIRS}
		${LUA_INCLUDE_DIR}
		${FREETYPE_INCLUDE_DIRS}
		${BOX3D_INCLUDE_DIRS}
		${GLAD_INCLUDE_DIR}
	PRIVATE
		${OPENGL_INCLUDE_DIRS}
		${SPIRV_TOOLING_INCLUDE_DIRS}
)

target_link_libraries(PyrosEngine
	PUBLIC
		${OPENGL_LIBS}
		${BOX3D_LIBRARIES}
		${FREETYPE_LIBRARIES}
		${LUA_LIBRARIES}
		${SPIRV_TOOLING_LIBS}
		${VULKAN_BACKEND_LIBS}
		${METAL_BACKEND_LIBS}
		${AUDIO_LIBS}
)

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

	# ImGui and glad are compiled into the engine but carry no PYROS3D_API -
	# they have export macros of their own instead. The demos and the editor
	# call both directly (the ImGui backends, and BaseExample/SDL2Context's
	# GL calls), so without this a SHARED build fails with LNK2001/LNK2019 on
	# every ImGui:: and glad_gl* symbol.
	#
	# PRIVATE vs INTERFACE is what splits export from import: PRIVATE reaches
	# only the engine's own translation units, INTERFACE only what links it.
	target_compile_definitions(PyrosEngine
		PRIVATE   "IMGUI_API=__declspec(dllexport)"
		INTERFACE "IMGUI_API=__declspec(dllimport)"

		# The ImGui *backends* straddle the boundary: the engine builds
		# imgui_impl_vulkan/metal, and each consumer builds imgui_impl_sdl2 +
		# imgui_impl_opengl3 (see PyrosWindowContext.cmake). Every one of them
		# is defined and called on the same side of the DLL, so their entry
		# points stay plain - IMGUI_IMPL_API defaults to IMGUI_API, and
		# leaving that to become dllimport in the consumer would reject the
		# definitions the consumer compiles itself (C2491).
		PUBLIC    "IMGUI_IMPL_API="

		# glad's equivalent, already wired through GLAPI in glad.h. Both sides
		# need GLAD_GLAPI_EXPORT; only the engine adds _BUILD, which is what
		# turns dllimport into dllexport. These are data symbols (the
		# glad_gl* function pointers), so the consumer genuinely needs
		# dllimport rather than merely benefiting from it.
		PUBLIC    GLAD_GLAPI_EXPORT
		PRIVATE   GLAD_GLAPI_EXPORT_BUILD
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
endif()

if (BUILD_EDITOR)
	add_subdirectory(${CMAKE_SOURCE_DIR}/editor)
endif()

if (BUILD_PLAYER)
	add_subdirectory(${CMAKE_SOURCE_DIR}/player)
endif()
