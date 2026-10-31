# PyrosPlayer — the runtime a built game ships with.
#
# Built via the root CMakeLists with -DBUILD_PLAYER=ON, inheriting
# PYROS_GRAPHICS / the dependency variables from the parent
# exactly like editor/ and examples/ do. It draws no editor UI of its own: a
# shipped game has no editor UI - see the note on the ImGui backend below.
#
# The editor's Build Game copies whichever PyrosPlayer binary this produces
# next to the game's assets, so this target is what a player ends up running.
#
# It does link ImGui's SDL2 backend, which a shipped game has no use for.
# That is not a choice: the shared window manager calls
# ImGui_ImplSDL2_ProcessEvent() from GetEvents() (guarded on there being a
# live ImGui context, which in a player there never is), and ImGui core is
# already inside PyrosEngine regardless. Forking the window manager to drop
# a call that never runs would cost more than the few KB it saves.

cmake_minimum_required(VERSION 3.11)

if (NOT DEFINED PYROS_CONTEXT)
	message(FATAL_ERROR
		"player/ must be built via the root CMakeLists with -DBUILD_PLAYER=ON "
		"(it inherits PYROS_GRAPHICS from the parent).")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/PyrosWindowContext.cmake)

add_executable(PyrosPlayer
	${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/PyrosPlayer.cpp
	${PYROS_IMGUI_BACKEND_SOURCE}
	${PYROS_WINDOW_MANAGER}
)

# A shipped game must not raise a console window behind itself. See
# cmake/PyrosWindows.cmake.
pyros_windows_gui_app(PyrosPlayer)

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

# The browser build: PyrosPlayer.js + .wasm, and the page that runs a game in
# them (player/web). Nothing of any one game is linked in. Build Game packs a
# game's files - its scenes and assets, and the engine's shaders - into one
# game.data beside the module, and the page writes that into the module's
# file system before main() runs, so the player finds game.json where it
# always does. This folder, built, is the "web" player template.
if (EMSCRIPTEN)
	pyros_emscripten_apply_compile(PyrosPlayer)
	pyros_emscripten_apply_link_no_shell(PyrosPlayer)
	target_link_options(PyrosPlayer PRIVATE
		# A desktop thread has 8 MB of stack and the engine is written for
		# it (scene loading recurses); emscripten's default is 64 KB.
		"SHELL:-sSTACK_SIZE=8MB"
		"SHELL:-sINITIAL_MEMORY=128MB"
		# What the page calls: the file system, to write the game into, and
		# the run dependency that holds main() until it has.
		"SHELL:-sEXPORTED_RUNTIME_METHODS=['FS','ENV','addRunDependency','removeRunDependency','ccall']"
		# A crash in a browser with neither of these is a list of addresses.
		# Names cost a little size and no speed; the assertions a little speed.
		"SHELL:--profiling-funcs"
		"SHELL:-sASSERTIONS=1"
	)
	foreach (page index.html player-shell.js manifest.webmanifest icon.png)
		add_custom_command(TARGET PyrosPlayer POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				${CMAKE_CURRENT_SOURCE_DIR}/web/${page}
				$<TARGET_FILE_DIR:PyrosPlayer>/${page})
	endforeach()
	# No dedicated server in a browser: it listens on a socket.
	return()
endif()

# PyrosServer - the same game, headless: no window, no GPU (no renderer made),
# for dedicated multiplayer servers. Built alongside the player; Build Game
# ships it next to PyrosPlayer.
add_executable(PyrosServer
	${CMAKE_CURRENT_SOURCE_DIR}/src/server_main.cpp
)
target_include_directories(PyrosServer PRIVATE
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/shared
)
target_link_libraries(PyrosServer PRIVATE
	PyrosEngine
	${BOX3D_LIBRARIES}
	${FREETYPE_LIBRARIES}
	${LUA_LIBRARIES}
)
if (APPLE)
	set_target_properties(PyrosServer PROPERTIES BUILD_RPATH "@executable_path")
elseif (UNIX)
	set_target_properties(PyrosServer PROPERTIES BUILD_RPATH "$ORIGIN")
endif()
if (WIN32 AND LIB_TYPE STREQUAL "SHARED")
	target_compile_definitions(PyrosServer PRIVATE _IMPORT)
endif()

# iOS: an app. SDL owns main (SDL2main starts UIKit and calls ours); the engine's shaders and the game -
# a built game's folder, -DPYROS_IOS_GAME=<folder with game.json> - go inside the bundle, which is
# where FindManifestDir looks (SDL_GetBasePath). Saves and the touch layout go to the app's own
# writable folder (SDL_GetPrefPath).
if (IOS)
	set(PYROS_IOS_GAME "" CACHE PATH "The built game (a folder with game.json) the iOS player app carries")
	set(PYROS_IOS_GAME_NAME "Game" CACHE STRING "The iOS app's name")
	set(PYROS_IOS_BUNDLE_ID "com.pyros3d.game" CACHE STRING "The iOS app's bundle identifier")
	set_target_properties(PyrosPlayer PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/ios/Info.plist.in
		MACOSX_BUNDLE_GUI_IDENTIFIER ${PYROS_IOS_BUNDLE_ID}
		MACOSX_BUNDLE_BUNDLE_NAME ${PYROS_IOS_GAME_NAME}
		MACOSX_BUNDLE_EXECUTABLE_NAME PyrosPlayer)
	target_link_libraries(PyrosPlayer PRIVATE SDL2main)
	add_custom_command(TARGET PyrosPlayer POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/examples/assets/middleclass.lua $<TARGET_BUNDLE_DIR:PyrosPlayer>/lua/middleclass.lua
		COMMENT "PyrosPlayer: its script runtime (lua/middleclass.lua) into the app")
	if (PYROS_IOS_GAME)
		add_custom_command(TARGET PyrosPlayer POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYROS_IOS_GAME} $<TARGET_BUNDLE_DIR:PyrosPlayer>
			COMMENT "PyrosPlayer: the game (${PYROS_IOS_GAME}) into the app")
	else()
		message(STATUS "iOS player: no PYROS_IOS_GAME - the app will carry no game")
	endif()
endif()
