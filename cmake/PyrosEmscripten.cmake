# Applied when configuring with emcmake / EMSCRIPTEN toolchain.
# Force WebGL2-compatible defaults and collect link/compile shell flags.

if (NOT EMSCRIPTEN)
	return()
endif()

message(STATUS "Pyros3D: Emscripten detected — targeting WebGL2 / GLES3")

# ---------------------------------------------------------------------------
# Force a web-sane option set (CACHE FORCE so GUI / -D leftovers can't fight it)
# ---------------------------------------------------------------------------
set(PYROS_GRAPHICS "OpenGL" CACHE STRING "Primary graphics backend" FORCE)
set(OPENGL_VERSION "GLES3" CACHE STRING "OpenGL/GLES profile for glad + shader #version" FORCE)
set(CONTEXT "SDL2" CACHE STRING "Window context" FORCE)
set(PYROS_CONTEXT "SDL2")

set(STATIC_LIB ON CACHE BOOL "Build PyrosEngine as a static library" FORCE)
set(LIB_TYPE STATIC)

set(BUILD_VULKAN_BACKEND OFF CACHE BOOL "Build VulkanRenderDevice" FORCE)
set(BUILD_SPIRV_TOOLING OFF CACHE BOOL "GLSL→SPIR-V tooling" FORCE)
set(BUILD_CONVERTER OFF CACHE BOOL "Build Assimp model converter tool" FORCE)

# Desktop-ish features (glad GLES loader path, DESKTOP define) stay useful on web.
set(IS_DESKTOP ON CACHE BOOL "Desktop target (vs embedded)" FORCE)

# ---------------------------------------------------------------------------
# Shared emscripten flags (SDL2 + Freetype ports, WebGL2, memory growth)
# ---------------------------------------------------------------------------
set(PYROS_EMSCRIPTEN_SHELL_FLAGS
	"SHELL:-sUSE_SDL=2"
	"SHELL:-sUSE_FREETYPE=1"
	"SHELL:-sFULL_ES3=1"
	"SHELL:-sUSE_WEBGL2=1"
	"SHELL:-sALLOW_MEMORY_GROWTH=1"
	"SHELL:-sDISABLE_EXCEPTION_CATCHING=0"
)

# Applied to every target that needs the ports' compile-time defines/includes.
function(pyros_emscripten_apply_compile target)
	target_compile_options(${target} PRIVATE ${PYROS_EMSCRIPTEN_SHELL_FLAGS})
	target_compile_definitions(${target} PRIVATE EMSCRIPTEN IMGUI_IMPL_OPENGL_ES3)
endfunction()

# Applied to classic demo executables (html/js/wasm output).
function(pyros_emscripten_apply_link target)
	target_link_options(${target} PRIVATE
		${PYROS_EMSCRIPTEN_SHELL_FLAGS}
		"SHELL:-sWASM=1"
		"SHELL:-sMIN_WEBGL_VERSION=2"
		"SHELL:-sMAX_WEBGL_VERSION=2"
		"SHELL:-lembind"
	)
	set_target_properties(${target} PROPERTIES SUFFIX ".html")
endfunction()

# Same as above, but the target brings its own page: emit only .js/.wasm/.data
# so emscripten's default shell is not left beside a real index.html for
# somebody to open by mistake (that one has no resize and no fullscreen).
function(pyros_emscripten_apply_link_no_shell target)
	pyros_emscripten_apply_link(${target})
	set_target_properties(${target} PROPERTIES SUFFIX ".js")
endfunction()

# JS-first library: MODULARIZE factory createPyros3D({ canvas }).
function(pyros_emscripten_apply_library_link target)
	target_link_options(${target} PRIVATE
		${PYROS_EMSCRIPTEN_SHELL_FLAGS}
		"SHELL:-sWASM=1"
		"SHELL:-sMIN_WEBGL_VERSION=2"
		"SHELL:-sMAX_WEBGL_VERSION=2"
		"SHELL:-lembind"
		"SHELL:-sMODULARIZE=1"
		"SHELL:-sEXPORT_NAME=createPyros3D"
		"SHELL:-sINVOKE_RUN=0"
		"SHELL:-sEXPORTED_RUNTIME_METHODS=['FS']"
	)
	set_target_properties(${target} PROPERTIES
		OUTPUT_NAME "Pyros3D"
		SUFFIX ".js"
	)
endfunction()
