# Examples — inherits options/deps/backend from the parent (BUILD_DEMOS).
# Do not re-find OpenGL/Bullet/Lua here; only resolve the window context.

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

set(EXAMPLES_PATH ${CMAKE_SOURCE_DIR}/examples)

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
		${BULLET_LIBRARIES}
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
endfunction()

# Minimal C++ API demo (ForwardRenderer + cube + light).
pyros_add_example(CppApiDemo
	DEMO_DEFINE _CppApiDemo
	SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/CppApiDemo/CppApiDemo.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
)

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

# Symlink resources/shaders into the examples build dir (live edits).
if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/shaders)
	file(CREATE_LINK ${CMAKE_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders SYMBOLIC)
endif()
