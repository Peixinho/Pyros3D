# Examples — inherits options/deps/backend from the parent (BUILD_DEMOS).
# Do not re-find OpenGL/Box3D/Lua here; only resolve the window context.

cmake_minimum_required(VERSION 3.11)

# When examples are built via add_subdirectory from the engine, parent
# already set PYROS_CONTEXT / VULKAN_BACKEND_FOUND / IMGUI_DIR / etc.
# Allow a standalone configure of this folder only if those exist.
if (NOT DEFINED PYROS_CONTEXT)
	message(FATAL_ERROR
		"examples/ must be built via the root CMakeLists with -DBUILD_DEMOS=ON "
		"(it inherits PYROS_GRAPHICS / CONTEXT / OPENGL_VERSION from the parent).")
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
	cmake_parse_arguments(ARG "" "DEMO_DEFINE" "SOURCES" ${ARGN})
	add_executable(${target_name}
		${ARG_SOURCES}
		${PYROS_IMGUI_BACKEND_SOURCE}
		${PYROS_WINDOW_MANAGER}
		${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
	)
	target_include_directories(${target_name} PRIVATE
		${CMAKE_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
		${SDL2_INCLUDE_DIRS}
		${IMGUI_INCLUDE_DIRS}
	)
	target_link_libraries(${target_name} PRIVATE
		PyrosEngine
		${PYROS_CONTEXT_LIB}
		${OPENGL_LIBS}
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
	if (WIN32 AND LIB_TYPE STREQUAL "SHARED")
		target_compile_definitions(${target_name} PRIVATE _IMPORT)
	endif()
	if (EMSCRIPTEN)
		pyros_emscripten_apply_compile(${target_name})
		pyros_emscripten_apply_link(${target_name})
		# Pack common assets + engine shaders into the virtual FS.
		target_link_options(${target_name} PRIVATE
			"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/examples/assets@assets"
			"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/resources/shaders@shaders"
		)
	endif()
endfunction()

# Minimal C++ API demo (ForwardRenderer + cube + light).
pyros_add_example(CppApiDemo
	DEMO_DEFINE _CppApiDemo
	SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/CppApiDemo/CppApiDemo.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
)

# Metal backend's first-light smoke test (see MetalHelloWindow.h) - a
# bespoke target, not pyros_add_example(): that helper always links the
# *build-wide* ${PYROS_WINDOW_MANAGER} (one window context per configure,
# chosen via PYROS_CONTEXT), so a normal -DPYROS_GRAPHICS=OpenGL/Vulkan
# configure would link the wrong window manager into anything using
# SDL2MetalContext directly. Building this explicitly against
# SDL2MetalContext.cpp regardless of the primary PYROS_CONTEXT means Metal
# bring-up can be verified without disturbing whatever the rest of this
# configure already builds (GL/Vulkan examples keep working unchanged) -
# only needs BUILD_METAL_BACKEND=ON, independent of PYROS_GRAPHICS/CONTEXT.
if (METAL_BACKEND_FOUND)
	add_executable(MetalHelloWindow
		${CMAKE_CURRENT_SOURCE_DIR}/MetalHelloWindow/MetalHelloWindow.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/WindowManagers/SDL2Metal/SDL2MetalContext.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
		${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	)
	find_package(SDL2 REQUIRED)
	target_include_directories(MetalHelloWindow PRIVATE
		${CMAKE_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
		${SDL2_INCLUDE_DIRS}
		${IMGUI_INCLUDE_DIRS}
	)
	target_link_libraries(MetalHelloWindow PRIVATE
		PyrosEngine
		$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,${SDL2_LIBRARIES}>
		${METAL_BACKEND_LIBS}
	)
	target_compile_definitions(MetalHelloWindow PRIVATE
		DEMO_NAME=MetalHelloWindow
		_MetalHelloWindow
		EXAMPLES_PATH=${EXAMPLES_PATH}
	)
	if (IS_DESKTOP)
		target_compile_definitions(MetalHelloWindow PRIVATE DESKTOP)
	endif()
endif()

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
		${OPENGL_LIBS}
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
	# Keep .data small: shaders + font + sfx still in VFS; game textures fetch over HTTP.
	target_link_options(Pyros3D PRIVATE
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/resources/shaders@shaders"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/examples/assets/verdana.ttf@assets/verdana.ttf"
		"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/examples/assets/neonpulse/sfx@assets/neonpulse/sfx"
	)
	# Serveable JS example next to Pyros3D.js / .wasm / .data
	add_custom_command(TARGET Pyros3D POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:Pyros3D>/web
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/web
			$<TARGET_FILE_DIR:Pyros3D>/web
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

# Stage resources/shaders into the examples build dir (a symlink, so edits
# are live; a copy on Windows - see pyros_stage_dir).
# Skip under Emscripten — shaders are --preload-file'd into the VFS instead.
if (NOT EMSCRIPTEN)
	pyros_stage_dir(${CMAKE_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders)
endif()
