# ---------------------------------------------------------------------------
# the device layer on WebGPU, natively: Dawn built from its source with Tint's SPIR-V reader on - the renderer's shaders, and
# the materials' surfaces compiled at run time, are SPIR-V, and the device hands WebGPU their WGSL
# (src/Pyros3D/Rendering/Device/SpirvToWgsl.cpp). One Tint: Dawn's own, in its static library, serves both (a
# Dawn release bundles a Tint without the reader, and a second Tint beside it would be two of everything).
#
# PYROS_DAWN_SOURCE / PYROS_DAWN_BUILD, else .fetchcontent-cache/dawn-src and dawn-build. Once:
#   git clone https://dawn.googlesource.com/dawn .fetchcontent-cache/dawn-src
#   cmake -S .fetchcontent-cache/dawn-src -B .fetchcontent-cache/dawn-build -DCMAKE_BUILD_TYPE=Release \
#     -DDAWN_FETCH_DEPENDENCIES=ON -DTINT_BUILD_SPV_READER=ON -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_CMD_TOOLS=OFF \
#     -DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_TESTS=OFF -DDAWN_USE_GLFW=OFF -DDAWN_BUILD_PROTOBUF=OFF \
#     -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC -DDAWN_ENABLE_INSTALL=OFF
#   cmake --build .fetchcontent-cache/dawn-build --target webgpu_dawn tint_api
# ---------------------------------------------------------------------------
function(pyros_add_webgpu target found)
	set(${found} OFF PARENT_SCOPE)
	if (EMSCRIPTEN)
		pyros_add_webgpu_browser(${target} browser)
		set(${found} ${browser} PARENT_SCOPE)
		set(PYROS_SPIRV2WGSL ${PYROS_SPIRV2WGSL} PARENT_SCOPE)
		return()
	endif()
	if (NOT PYROS_DAWN_SOURCE)
		set(PYROS_DAWN_SOURCE ${CMAKE_SOURCE_DIR}/.fetchcontent-cache/dawn-src)
	endif()
	if (NOT PYROS_DAWN_BUILD)
		set(PYROS_DAWN_BUILD ${CMAKE_SOURCE_DIR}/.fetchcontent-cache/dawn-build)
	endif()
	file(GLOB_RECURSE DAWN_LIBRARY ${PYROS_DAWN_BUILD}/src/dawn/native/libwebgpu_dawn.a)
	set(DAWN_HEADER ${PYROS_DAWN_BUILD}/gen/include/dawn/webgpu.h)     # (the generated one: <webgpu/webgpu.h> in the source includes it)
	if (NOT DAWN_LIBRARY OR NOT EXISTS ${DAWN_HEADER} OR NOT EXISTS ${PYROS_DAWN_SOURCE}/src/tint/lang/spirv/reader/reader.h)
		message(STATUS "WebGPU device: off - Dawn built with Tint's SPIR-V reader wanted at PYROS_DAWN_BUILD (see cmake/PyrosWebGPU.cmake)")
		return()
	endif()
	list(GET DAWN_LIBRARY 0 DAWN_LIBRARY)
	set(DAWN_INCLUDE ${PYROS_DAWN_BUILD}/gen/include)
	# (the libraries Dawn's own are made of beside its monolith: Tint's pieces and what they stand on)
	file(GLOB_RECURSE DAWN_DEPS ${PYROS_DAWN_BUILD}/third_party/spirv-tools/*.a ${PYROS_DAWN_BUILD}/third_party/abseil/*.a)

	set(D2 ${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device)
	target_sources(${target} PRIVATE ${D2}/WebGPUDevice.cpp ${D2}/SpirvToWgsl.cpp)
	target_include_directories(${target} PRIVATE ${DAWN_INCLUDE} ${PYROS_DAWN_SOURCE}/include)
	# (Tint's own headers want C++20 and its build's switches)
	set_source_files_properties(${D2}/SpirvToWgsl.cpp PROPERTIES
		COMPILE_OPTIONS "-std=c++20"
		INCLUDE_DIRECTORIES "${PYROS_DAWN_SOURCE};${PYROS_DAWN_BUILD}/gen/src"
		COMPILE_DEFINITIONS "NDEBUG;DAWN_HAVE_UTILS_BUILD_CONFIG;TINT_BUILD_SPV_READER=1;TINT_BUILD_WGSL_READER=1;TINT_BUILD_WGSL_WRITER=1;TINT_BUILD_IR_BINARY=0;TINT_BUILD_IS_MAC=$<BOOL:${APPLE}>;TINT_BUILD_IS_LINUX=$<AND:$<BOOL:${UNIX}>,$<NOT:$<BOOL:${APPLE}>>>;TINT_BUILD_IS_WIN=$<BOOL:${WIN32}>")
	target_link_libraries(${target} PUBLIC ${DAWN_LIBRARY} ${DAWN_DEPS})
	if (APPLE)
		target_link_libraries(${target} PUBLIC "-framework Foundation" "-framework IOSurface" "-framework QuartzCore" "-framework Cocoa" "-framework IOKit" "-framework Metal" "-framework CoreFoundation")
	endif()
	target_compile_definitions(${target} PUBLIC PYROS_GFX_WEBGPU PRIVATE PYROS_GFX_SPIRV_TO_WGSL)
	# (the same SPIR-V to WGSL as a tool: the browser build's shaders, made at its build time - PYROS_SPIRV2WGSL there)
	add_executable(pyros_spirv2wgsl ${CMAKE_SOURCE_DIR}/tools/spirv2wgsl/main.cpp ${D2}/SpirvToWgsl.cpp)
	target_include_directories(pyros_spirv2wgsl PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_link_libraries(pyros_spirv2wgsl PRIVATE ${DAWN_LIBRARY} ${DAWN_DEPS})
	if (APPLE)
		target_link_libraries(pyros_spirv2wgsl PRIVATE "-framework Foundation" "-framework IOSurface" "-framework QuartzCore" "-framework Cocoa" "-framework IOKit" "-framework Metal" "-framework CoreFoundation")
	endif()
	set(${found} ON PARENT_SCOPE)
	message(STATUS "WebGPU device: on - Dawn ${DAWN_LIBRARY}")
endfunction()

# ---------------------------------------------------------------------------
# The browser's WebGPU (an Emscripten build): emdawnwebgpu - Dawn's webgpu.h over the browser's
# WebGPU - built from the same Dawn source with Emscripten (no download): PYROS_DAWN_WASM_BUILD, else
# .fetchcontent-cache/tint-wasm. Once:
#   emcmake cmake -S .fetchcontent-cache/dawn-src -B .fetchcontent-cache/tint-wasm -DCMAKE_BUILD_TYPE=Release \
#     -DDAWN_FETCH_DEPENDENCIES=ON -DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_TESTS=OFF -DTINT_BUILD_TESTS=OFF
#   cmake --build .fetchcontent-cache/tint-wasm --target emdawnwebgpu_pkg
# The page turns no SPIR-V into WGSL and compiles no GLSL: the renderer's shaders are WGSL made at build time by the
# native pyros_spirv2wgsl (PYROS_SPIRV2WGSL: a native build's), the materials' surfaces WGSL a native run wrote into
# the surface cache (SceneRenderer::SetSurfaceCache).
# ---------------------------------------------------------------------------
function(pyros_add_webgpu_browser target found)
	set(${found} OFF PARENT_SCOPE)
	if (NOT PYROS_WEB_WEBGPU)
		return()
	endif()
	if (NOT PYROS_DAWN_WASM_BUILD)
		set(PYROS_DAWN_WASM_BUILD ${CMAKE_SOURCE_DIR}/.fetchcontent-cache/tint-wasm)
	endif()
	if (NOT PYROS_SPIRV2WGSL)
		set(PYROS_SPIRV2WGSL ${CMAKE_SOURCE_DIR}/build_ed_mtl/pyros_spirv2wgsl)
	endif()
	set(PORT ${PYROS_DAWN_WASM_BUILD}/emdawnwebgpu_pkg/emdawnwebgpu.port.py)
	if (NOT EXISTS ${PORT} OR NOT EXISTS ${PYROS_SPIRV2WGSL})
		message(FATAL_ERROR "PYROS_WEB_WEBGPU: emdawnwebgpu at ${PYROS_DAWN_WASM_BUILD} and a native pyros_spirv2wgsl (PYROS_SPIRV2WGSL) wanted - see cmake/PyrosWebGPU.cmake")
	endif()
	set(PYROS_SPIRV2WGSL ${PYROS_SPIRV2WGSL} PARENT_SCOPE)
	target_sources(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device/WebGPUDevice.cpp)
	# (the port on every compile that sees webgpu.h, and on the link of whatever links the engine; Asyncify: the device
	# waits for its adapter and device once, when it is made)
	target_compile_options(${target} PUBLIC "SHELL:--use-port=${PORT}")
	target_link_options(${target} PUBLIC "SHELL:--use-port=${PORT}" "SHELL:-sASYNCIFY=1" "SHELL:-sASYNCIFY_STACK_SIZE=1048576")
	target_compile_definitions(${target} PUBLIC PYROS_GFX_WEBGPU PRIVATE PYROS_RENDERER_WGSL)
	set(${found} ON PARENT_SCOPE)
	message(STATUS "WebGPU device: on - the browser's, through emdawnwebgpu; shaders' WGSL by ${PYROS_SPIRV2WGSL}")
endfunction()
