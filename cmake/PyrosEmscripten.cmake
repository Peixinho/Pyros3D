# Applied when configuring with emcmake / EMSCRIPTEN toolchain: the browser build, on WebGPU (the renderer on
# the device layer - cmake/PyrosWebGPU.cmake). Collects the link/compile shell flags.

if (NOT EMSCRIPTEN)
	return()
endif()

message(STATUS "Pyros3D: Emscripten detected - targeting WebGPU")
set(PYROS_WEB_WEBGPU ON CACHE BOOL "The browser build on WebGPU" FORCE)

# ---------------------------------------------------------------------------
# Force a web-sane option set (CACHE FORCE so GUI / -D leftovers can't fight it)
# ---------------------------------------------------------------------------
set(PYROS_GRAPHICS "WebGPU" CACHE STRING "The GPU API the renderer draws with" FORCE)

set(STATIC_LIB ON CACHE BOOL "Build PyrosEngine as a static library" FORCE)
set(LIB_TYPE STATIC)

set(BUILD_VULKAN_BACKEND OFF CACHE BOOL "Build the Vulkan device" FORCE)
set(BUILD_METAL_BACKEND OFF CACHE BOOL "Build the Metal device" FORCE)
set(BUILD_CONVERTER OFF CACHE BOOL "Build Assimp model converter tool" FORCE)

set(IS_DESKTOP ON CACHE BOOL "Desktop target (vs embedded)" FORCE)

# ---------------------------------------------------------------------------
# Shared emscripten flags (SDL2 + Freetype ports, memory growth)
# ---------------------------------------------------------------------------
set(PYROS_EMSCRIPTEN_SHELL_FLAGS
	"SHELL:-sUSE_SDL=2"
	"SHELL:-sUSE_FREETYPE=1"
	"SHELL:-sALLOW_MEMORY_GROWTH=1"
	"SHELL:-sDISABLE_EXCEPTION_CATCHING=0"
	# (what the script store's EM_ASM calls to hand a string back - PyrosLuaMisc.cpp)
	"SHELL:-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$stringToNewUTF8"
)

# Applied to every target that needs the ports' compile-time defines/includes.
function(pyros_emscripten_apply_compile target)
	target_compile_options(${target} PRIVATE ${PYROS_EMSCRIPTEN_SHELL_FLAGS})
	target_compile_definitions(${target} PRIVATE EMSCRIPTEN)
endfunction()

# Applied to classic demo executables (html/js/wasm output).
function(pyros_emscripten_apply_link target)
	target_link_options(${target} PRIVATE
		${PYROS_EMSCRIPTEN_SHELL_FLAGS}
		"SHELL:-sWASM=1"
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
