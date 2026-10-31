# Examples — inherits options/deps/backend from the parent (BUILD_DEMOS).
# Do not re-find OpenGL/Box3D/Lua here; only resolve the window context.

cmake_minimum_required(VERSION 3.11)

# When examples are built via add_subdirectory from the engine, parent
# already set PYROS_CONTEXT / VULKAN_BACKEND_FOUND / IMGUI_DIR / etc.
# Allow a standalone configure of this folder only if those exist.
if (NOT DEFINED PYROS_CONTEXT)
	message(FATAL_ERROR
		"examples/ must be built via the root CMakeLists with -DBUILD_DEMOS=ON "
		"(it inherits PYROS_GRAPHICS from the parent).")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/PyrosWindowContext.cmake)

# See PYROS_ASSET_ROOT in cmake/PyrosOptions.cmake - absolute source
# path for a local build, "." for a relocatable one.
set(EXAMPLES_PATH ${PYROS_ASSET_ROOT})

# freebsd legacy search path
if (EXISTS "/usr/local/lib")
	link_directories("/usr/local/lib")
endif()

# Helper: one example executable with the shared window + ImGui backends.
function(pyros_add_example target_name)
	cmake_parse_arguments(ARG "SHADERS_ONLY" "DEMO_DEFINE" "SOURCES" ${ARGN})
	add_executable(${target_name}
		${ARG_SOURCES}
		${PYROS_IMGUI_BACKEND_SOURCE}
		${PYROS_WINDOW_MANAGER}
		${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
	)
	target_include_directories(${target_name} PRIVATE
		${CMAKE_SOURCE_DIR}/include
		# shared/ holds the file formats that live outside the engine - the
		# prefab and .uistyle resolvers the editor and the player also use.
		${CMAKE_SOURCE_DIR}/shared
		${CMAKE_CURRENT_SOURCE_DIR}
		${SDL2_INCLUDE_DIRS}
		${IMGUI_INCLUDE_DIRS}
	)
	target_link_libraries(${target_name} PRIVATE
		PyrosEngine
		${PYROS_CONTEXT_LIB}
		${BOX3D_LIBRARIES}
		${FREETYPE_LIBRARIES}
		${LUA_LIBRARIES}
	)
	target_compile_definitions(${target_name} PRIVATE
		DEMO_NAME=${target_name}
		${ARG_DEMO_DEFINE}
		${PYROS_CONTEXT_DEFINITION}
		EXAMPLES_PATH=${EXAMPLES_PATH}
	)
	if (IS_DESKTOP)
		target_compile_definitions(${target_name} PRIVATE DESKTOP)
	endif()
	# iOS: an app - SDL owns main (SDL2main starts UIKit and calls ours), and the
	# shaders and the demos' assets go inside the bundle, where it runs from.
	if (IOS)
		set_target_properties(${target_name} PROPERTIES
			MACOSX_BUNDLE TRUE
			MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/ios/Info.plist.in
			MACOSX_BUNDLE_GUI_IDENTIFIER com.pyros3d.${target_name}
			MACOSX_BUNDLE_BUNDLE_NAME ${target_name}
			MACOSX_BUNDLE_EXECUTABLE_NAME ${target_name})
		target_link_libraries(${target_name} PRIVATE SDL2main)
		add_custom_command(TARGET ${target_name} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_BUNDLE_DIR:${target_name}>/assets
			COMMENT "${target_name}: its assets into the app")
	endif()
	if (WIN32 AND LIB_TYPE STREQUAL "SHARED")
		target_compile_definitions(${target_name} PRIVATE _IMPORT)
	endif()
	if (EMSCRIPTEN)
		pyros_emscripten_apply_compile(${target_name})
		pyros_emscripten_apply_link(${target_name})
		# The common assets in the virtual FS (a demo that builds its own geometry - SHADERS_ONLY - needs none: the
		# renderer's shaders are in the engine)
		if (NOT ARG_SHADERS_ONLY)
			target_link_options(${target_name} PRIVATE
				"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/examples/assets@assets"
			)
		endif()
	endif()
endfunction()

# JS-first Embind library: import createPyros3D and drive the engine from JS.
# (Replaces the old C++-host JsApiDemo + pyrosInit pattern.)
if (EMSCRIPTEN)
	add_executable(Pyros3D
		${CMAKE_CURRENT_SOURCE_DIR}/web/native/main.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/web/native/JsApplication.cpp
		${PYROS_WINDOW_MANAGER}
		${PYROS_IMGUI_BACKEND_SOURCE}
	)
	target_include_directories(Pyros3D PRIVATE
		${CMAKE_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/web/native
		${SDL2_INCLUDE_DIRS}
		${IMGUI_INCLUDE_DIRS}
	)
	target_link_libraries(Pyros3D PRIVATE
		PyrosEngine
		${PYROS_CONTEXT_LIB}
		${BOX3D_LIBRARIES}
		${FREETYPE_LIBRARIES}
	)
	target_compile_definitions(Pyros3D PRIVATE
		${PYROS_CONTEXT_DEFINITION}
		EXAMPLES_PATH=${EXAMPLES_PATH}
	)
	if (IS_DESKTOP)
		target_compile_definitions(Pyros3D PRIVATE DESKTOP)
	endif()
	pyros_emscripten_apply_compile(Pyros3D)
	pyros_emscripten_apply_library_link(Pyros3D)
	# Keep .data small: font + sfx in the VFS; game textures fetch over HTTP.
	target_link_options(Pyros3D PRIVATE
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/examples/assets/verdana.ttf@assets/verdana.ttf"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/examples/assets/neonpulse/sfx@assets/neonpulse/sfx"
	)
	# Serveable JS example next to Pyros3D.js / .wasm / .data
	add_custom_command(TARGET Pyros3D POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:Pyros3D>/web
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/web
			$<TARGET_FILE_DIR:Pyros3D>/web
		# web/native is the C++ that produced the module, not part of what is
		# served. The staged folder is the deliverable now, so it should not
		# carry sources no browser will ever ask for.
		COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:Pyros3D>/web/native
		COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Pyros3D>/web/assets
		COMMAND ${CMAKE_COMMAND} -E copy
			${CMAKE_SOURCE_DIR}/examples/assets/smoke.png
			$<TARGET_FILE_DIR:Pyros3D>/web/assets/smoke.png
		# The module next to the page that loads it. web/ is what gets zipped
		# up and served, so it has to stand on its own: index.html asks for
		# ./Pyros3D.js, and Emscripten then resolves the .wasm/.data beside
		# it. These are link outputs, so they already exist by POST_BUILD.
		COMMAND ${CMAKE_COMMAND} -E copy
			$<TARGET_FILE_DIR:Pyros3D>/Pyros3D.js
			$<TARGET_FILE_DIR:Pyros3D>/Pyros3D.wasm
			$<TARGET_FILE_DIR:Pyros3D>/Pyros3D.data
			$<TARGET_FILE_DIR:Pyros3D>/web/
		COMMENT "Copy examples/web (+ HTTP assets + the module) into build output"
	)
endif()

# Unified demo browser (JSON + Lua) — needs Lua bindings.
if (HAVE_LUA_BINDINGS)
	pyros_add_example(DemoLauncher
		DEMO_DEFINE _DemoLauncher
		SOURCES
			${CMAKE_CURRENT_SOURCE_DIR}/DemoLauncher/DemoLauncher.cpp
	)
else()
	message(STATUS "DemoLauncher skipped (HAVE_LUA_BINDINGS=OFF)")
endif()

