# PyrosPlayer — the runtime a built game ships with.
#
# Built via the root CMakeLists with -DBUILD_PLAYER=ON, inheriting
# PYROS_CONTEXT / OPENGL_VERSION / the dependency variables from the parent
# exactly like editor/ and examples/ do. It draws no editor UI of its own: a
# shipped game has no editor UI - see the note on the ImGui backend below.
#
# The editor's Build Game copies whichever PyrosPlayer binary this produces
# next to the game's assets, so this target is what a player ends up running.
#
# It does link ImGui's SDL2 backend, which a shipped game has no use for.
# That is not a choice: the shared SDL2/Vulkan/Metal window managers call
# ImGui_ImplSDL2_ProcessEvent() from GetEvents() (guarded on there being a
# live ImGui context, which in a player there never is), and ImGui core is
# already inside PyrosEngine regardless. Forking the window manager to drop
# a call that never runs would cost more than the few KB it saves.

cmake_minimum_required(VERSION 3.11)

if (NOT DEFINED PYROS_CONTEXT)
	message(FATAL_ERROR
		"player/ must be built via the root CMakeLists with -DBUILD_PLAYER=ON "
		"(it inherits PYROS_GRAPHICS / CONTEXT / OPENGL_VERSION from the parent).")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/PyrosWindowContext.cmake)

add_executable(PyrosPlayer
	${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/PyrosPlayer.cpp
	${PYROS_IMGUI_BACKEND_SOURCE}
	${PYROS_WINDOW_MANAGER}
)

target_include_directories(PyrosPlayer PRIVATE
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/src
	# WindowManagers/ (not .../SDL2) so the context include in PyrosPlayer.h
	# resolves to the same one the editor and examples use.
	${CMAKE_SOURCE_DIR}/examples/WindowManagers
	# Prefab resolution, shared with the editor and deliberately outside the
	# engine (see shared/PrefabResolver.h).
	${CMAKE_SOURCE_DIR}/shared
	${SDL2_INCLUDE_DIRS}
	${IMGUI_INCLUDE_DIRS}
)

target_link_libraries(PyrosPlayer PRIVATE
	PyrosEngine
	${PYROS_CONTEXT_LIB}
	${OPENGL_LIBS}
	${BOX3D_LIBRARIES}
	${FREETYPE_LIBRARIES}
	${LUA_LIBRARIES}
)

target_compile_definitions(PyrosPlayer PRIVATE ${PYROS_CONTEXT_DEFINITION})

# The engine is a shared library, and a built game has it sitting next to
# the player rather than in the build tree it was linked against. Adding the
# executable's own directory to the rpath is what makes the copied binary
# find the copied library - without it, a build runs on the machine that
# produced it and nowhere else.
if (APPLE)
	set_target_properties(PyrosPlayer PROPERTIES BUILD_RPATH "@executable_path")
elseif (UNIX)
	set_target_properties(PyrosPlayer PROPERTIES BUILD_RPATH "$ORIGIN")
endif()

if (IS_DESKTOP)
	target_compile_definitions(PyrosPlayer PRIVATE DESKTOP)
endif()

if (WIN32 AND LIB_TYPE STREQUAL "SHARED")
	target_compile_definitions(PyrosPlayer PRIVATE _IMPORT)
endif()
