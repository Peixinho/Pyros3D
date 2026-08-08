# OpenGL glad + Vulkan backend + SPIR-V tooling. Expects options + deps loaded.

set(PYROS_EXT_DIR ${CMAKE_SOURCE_DIR}/src/Pyros3D/Ext)

# ---------------------------------------------------------------------------
# OpenGL / GLES (glad)
# ---------------------------------------------------------------------------
if (OPENGL_VERSION STREQUAL "GL45")
	add_compile_definitions(GL45)
	set(GL_INCLUDE ${PYROS_EXT_DIR}/gl45/glad.c)
	set(GLAD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/Pyros3D/Ext/gl45)
	find_package(OpenGL REQUIRED)
	set(OPENGL_LIBS ${OPENGL_LIBRARIES})
elseif (OPENGL_VERSION STREQUAL "GL42")
	add_compile_definitions(GL42)
	set(GL_INCLUDE ${PYROS_EXT_DIR}/gl42/glad.c)
	set(GLAD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/Pyros3D/Ext/gl42)
	find_package(OpenGL REQUIRED)
	set(OPENGL_LIBS ${OPENGL_LIBRARIES})
elseif (OPENGL_VERSION STREQUAL "GL41")
	add_compile_definitions(GL41)
	set(GL_INCLUDE ${PYROS_EXT_DIR}/gl41/glad.c)
	set(GLAD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/Pyros3D/Ext/gl41)
	find_package(OpenGL REQUIRED)
	set(OPENGL_LIBS ${OPENGL_LIBRARIES})
elseif (OPENGL_VERSION STREQUAL "GLES3")
	add_compile_definitions(GLES3)
	# ImGui OpenGL3 backend must use ES shaders on GLES / WebGL2.
	add_compile_definitions(IMGUI_IMPL_OPENGL_ES3)
	# Always use glad GLES3 (desktop ES, Emscripten/WebGL2, Raspberry Pi / embedded).
	set(GL_INCLUDE ${PYROS_EXT_DIR}/gles3/glad.c)
	set(GLAD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/Pyros3D/Ext/gles3)
	if (EMSCRIPTEN)
		# Browser / emscripten GL; do not link system GLESv2.
		set(OPENGL_LIBS "")
	elseif (ANDROID)
		# NDK: GLESv3 is preferred; GLESv2 + EGL still required for loader symbols.
		find_library(PYROS_GLESV3_LIBRARY GLESv3)
		find_library(PYROS_GLESV2_LIBRARY GLESv2 REQUIRED)
		find_library(PYROS_EGL_LIBRARY EGL REQUIRED)
		find_library(PYROS_ANDROID_LIBRARY android REQUIRED)
		find_library(PYROS_LOG_LIBRARY log REQUIRED)
		set(OPENGL_LIBS ${PYROS_GLESV2_LIBRARY} ${PYROS_EGL_LIBRARY} ${PYROS_ANDROID_LIBRARY} ${PYROS_LOG_LIBRARY})
		if (PYROS_GLESV3_LIBRARY)
			list(APPEND OPENGL_LIBS ${PYROS_GLESV3_LIBRARY})
		endif()
		message(STATUS "GLES3 Android libs: ${OPENGL_LIBS}")
	else()
		# Mesa / Pi OS: libGLESv2.so + libEGL.so (not the legacy Broadcom /opt/vc stack).
		find_library(PYROS_GLESV2_LIBRARY NAMES GLESv2 libGLESv2 REQUIRED)
		find_library(PYROS_EGL_LIBRARY NAMES EGL libEGL REQUIRED)
		set(OPENGL_LIBS ${PYROS_GLESV2_LIBRARY} ${PYROS_EGL_LIBRARY})
		message(STATUS "GLES3 libs: GLESv2=${PYROS_GLESV2_LIBRARY} EGL=${PYROS_EGL_LIBRARY}")
	endif()
else()
	message(FATAL_ERROR "Unknown OPENGL_VERSION='${OPENGL_VERSION}' (expected GL45, GL42, GL41, or GLES3)")
endif()

if (EMSCRIPTEN AND NOT OPENGL_VERSION STREQUAL "GLES3")
	message(FATAL_ERROR "Emscripten builds require OPENGL_VERSION=GLES3 (WebGL2)")
endif()

# ---------------------------------------------------------------------------
# SPIR-V tooling (shaderc + spirv-cross) — used by Vulkan GLSL→SPIR-V path
# ---------------------------------------------------------------------------
set(SPIRV_TOOLING_FOUND OFF)
set(SPIRV_TOOLING_SOURCE "")
set(SPIRV_TOOLING_INCLUDE_DIRS "")
set(SPIRV_TOOLING_LIBS "")

if (BUILD_SPIRV_TOOLING)
	if (APPLE)
		execute_process(COMMAND brew --prefix shaderc
			OUTPUT_VARIABLE HOMEBREW_SHADERC_PREFIX
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		execute_process(COMMAND brew --prefix spirv-cross
			OUTPUT_VARIABLE HOMEBREW_SPIRV_CROSS_PREFIX
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
	endif()

	find_path(SHADERC_INCLUDE_DIR shaderc/shaderc.hpp PATHS ${HOMEBREW_SHADERC_PREFIX}/include)
	# Prefer the shared lib: Arch's libshaderc_combined.a is a thin archive that
	# still needs glslang + SPIRV-Tools at link time (Homebrew's combined is fat).
	find_library(SHADERC_LIBRARY NAMES shaderc_shared shaderc shaderc_combined
		PATHS ${HOMEBREW_SHADERC_PREFIX}/lib)
	find_path(SPIRV_CROSS_INCLUDE_DIR spirv_cross/spirv_cross.hpp PATHS ${HOMEBREW_SPIRV_CROSS_PREFIX}/include)
	find_library(SPIRV_CROSS_CORE_LIBRARY NAMES spirv-cross-core PATHS ${HOMEBREW_SPIRV_CROSS_PREFIX}/lib)
	find_library(SPIRV_CROSS_GLSL_LIBRARY NAMES spirv-cross-glsl PATHS ${HOMEBREW_SPIRV_CROSS_PREFIX}/lib)
	find_library(SPIRV_CROSS_CPP_LIBRARY NAMES spirv-cross-cpp PATHS ${HOMEBREW_SPIRV_CROSS_PREFIX}/lib)

	if (SHADERC_INCLUDE_DIR AND SHADERC_LIBRARY AND SPIRV_CROSS_INCLUDE_DIR
		AND SPIRV_CROSS_CORE_LIBRARY AND SPIRV_CROSS_GLSL_LIBRARY AND SPIRV_CROSS_CPP_LIBRARY)
		set(SPIRV_TOOLING_FOUND ON)
		add_compile_definitions(SPIRV_TOOLING)
		set(SPIRV_TOOLING_SOURCE ${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/SPIRV/ShaderCompiler.cpp)
		set(SPIRV_TOOLING_INCLUDE_DIRS ${SHADERC_INCLUDE_DIR} ${SPIRV_CROSS_INCLUDE_DIR})
		set(SPIRV_TOOLING_LIBS
			${SHADERC_LIBRARY}
			${SPIRV_CROSS_CPP_LIBRARY}
			${SPIRV_CROSS_GLSL_LIBRARY}
			${SPIRV_CROSS_CORE_LIBRARY}
		)
		# Thin shaderc_combined.a (e.g. Arch) leaves glslang/SPIRV-Tools unresolved.
		if (SHADERC_LIBRARY MATCHES "combined" OR SHADERC_LIBRARY MATCHES "\\.(a|lib)$")
			find_library(GLSLANG_LIBRARY NAMES glslang)
			find_library(GLSLANG_SPIRV_LIBRARY NAMES SPIRV)
			find_library(SPIRV_TOOLS_OPT_LIBRARY NAMES SPIRV-Tools-opt)
			find_library(SPIRV_TOOLS_LIBRARY NAMES SPIRV-Tools-shared SPIRV-Tools)
			if (GLSLANG_LIBRARY)
				list(APPEND SPIRV_TOOLING_LIBS ${GLSLANG_LIBRARY})
			endif()
			if (GLSLANG_SPIRV_LIBRARY)
				list(APPEND SPIRV_TOOLING_LIBS ${GLSLANG_SPIRV_LIBRARY})
			endif()
			if (SPIRV_TOOLS_OPT_LIBRARY)
				list(APPEND SPIRV_TOOLING_LIBS ${SPIRV_TOOLS_OPT_LIBRARY})
			endif()
			if (SPIRV_TOOLS_LIBRARY)
				list(APPEND SPIRV_TOOLING_LIBS ${SPIRV_TOOLS_LIBRARY})
			endif()
		endif()
		message(STATUS "SPIR-V tooling: shaderc=${SHADERC_LIBRARY}")
	else()
		message(WARNING
			"shaderc/spirv-cross not found — SPIR-V tooling disabled. "
			"Install shaderc + spirv-cross (e.g. pacman -S shaderc spirv-cross, "
			"apt install libshaderc-dev spirv-cross, brew install shaderc spirv-cross).")
	endif()
endif()

# ---------------------------------------------------------------------------
# ImGui core (single copy in the engine)
# ---------------------------------------------------------------------------
set(IMGUI_DIR ${PYROS_EXT_DIR}/imgui)
set(IMGUI_CORE_SOURCE
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
	${IMGUI_DIR}/imgui_tables.cpp
)
set(IMGUI_INCLUDE_DIRS ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# ---------------------------------------------------------------------------
# Vulkan backend (optional)
# ---------------------------------------------------------------------------
set(VULKAN_BACKEND_FOUND OFF)
set(VULKAN_BACKEND_SOURCE "")
set(VULKAN_BACKEND_LIBS "")

if (BUILD_VULKAN_BACKEND)
	find_package(Vulkan)

	if (Vulkan_FOUND)
		include(FetchContent)
		FetchContent_Declare(
			volk
			GIT_REPOSITORY https://github.com/zeux/volk.git
			GIT_TAG vulkan-sdk-1.4.357.0
		)
		FetchContent_MakeAvailable(volk)
		if (TARGET volk AND NOT TARGET volk::volk)
			add_library(volk::volk ALIAS volk)
		endif()

		FetchContent_Declare(
			VulkanMemoryAllocator
			GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
			GIT_TAG v3.3.0
		)
		FetchContent_MakeAvailable(VulkanMemoryAllocator)

		set(VULKAN_BACKEND_FOUND ON)
		add_compile_definitions(VULKAN_BACKEND)
		add_compile_definitions(IMGUI_IMPL_VULKAN_USE_VOLK)
		set(VULKAN_BACKEND_SOURCE
			${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device/VulkanRenderDevice.cpp
			${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device/VulkanImGuiBackend.cpp
			${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
		)
		set(VULKAN_BACKEND_LIBS Vulkan::Vulkan volk::volk GPUOpen::VulkanMemoryAllocator)
		message(STATUS "Vulkan backend: Vulkan=${Vulkan_LIBRARY}, volk+VMA via FetchContent")
	else()
		message(WARNING "Vulkan SDK not found — Vulkan backend disabled. Install LunarG SDK or: brew install vulkan-headers vulkan-loader")
		if (PYROS_CONTEXT STREQUAL "SDL2Vulkan")
			message(FATAL_ERROR "SDL2Vulkan context was selected but Vulkan SDK is missing")
		endif()
	endif()
endif()

# ---------------------------------------------------------------------------
# Metal backend (optional, Apple only)
# ---------------------------------------------------------------------------
set(METAL_BACKEND_FOUND OFF)
set(METAL_BACKEND_SOURCE "")
set(METAL_BACKEND_LIBS "")

if (BUILD_METAL_BACKEND)
	find_library(METAL_FRAMEWORK Metal)
	find_library(QUARTZCORE_FRAMEWORK QuartzCore)
	find_library(FOUNDATION_FRAMEWORK Foundation)

	if (METAL_FRAMEWORK AND QUARTZCORE_FRAMEWORK AND FOUNDATION_FRAMEWORK)
		# MetalRenderDevice.mm is Objective-C++ (talks to real id<MTLXxx>
		# types, unlike everything else in this engine) - CMake only knows
		# to compile a .mm source with the Objective-C++ frontend once this
		# language is enabled at least once per configure. Harmless to call
		# more than once/redundantly with other CMake files that might also
		# need it later (e.g. a real ImGui-on-Metal backend).
		enable_language(OBJCXX)
		set(METAL_BACKEND_FOUND ON)
		add_compile_definitions(METAL_BACKEND)
		set(METAL_BACKEND_SOURCE
			${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device/MetalRenderDevice.mm
		)
		# ARC only for this file - it's the one place that stores id<MTLXxx>
		# objects across calls (as CFBridgingRetain()'d void* members, see
		# its header comment); everywhere else in this engine is plain C++
		# with no Objective-C object lifetimes to manage.
		set_source_files_properties(
			${CMAKE_SOURCE_DIR}/src/Pyros3D/Rendering/Device/MetalRenderDevice.mm
			PROPERTIES COMPILE_FLAGS "-fobjc-arc"
		)
		set(METAL_BACKEND_LIBS ${METAL_FRAMEWORK} ${QUARTZCORE_FRAMEWORK} ${FOUNDATION_FRAMEWORK})

		# GLSL -> SPIR-V (shaderc, already found above for the Vulkan path)
		# -> MSL (spirv-cross-msl, the one spirv-cross backend library the
		# Vulkan path never needed - it only ever reflects SPIR-V, never
		# regenerates shader *source* from it). Real shader compilation is
		# unavailable without both; MetalRenderDevice::CompileShaderStage()
		# checks METAL_SHADER_TOOLING itself and fails loudly rather than
		# silently, same pattern as VulkanRenderDevice's SPIRV_TOOLING check.
		if (SPIRV_TOOLING_FOUND)
			find_library(SPIRV_CROSS_MSL_LIBRARY NAMES spirv-cross-msl PATHS ${HOMEBREW_SPIRV_CROSS_PREFIX}/lib)
			if (SPIRV_CROSS_MSL_LIBRARY)
				add_compile_definitions(METAL_SHADER_TOOLING)
				list(APPEND METAL_BACKEND_LIBS ${SPIRV_CROSS_MSL_LIBRARY})
				message(STATUS "Metal shader tooling: spirv-cross-msl=${SPIRV_CROSS_MSL_LIBRARY}")
			else()
				message(WARNING "spirv-cross-msl not found — MetalRenderDevice will fail to compile any shader (brew install spirv-cross should already provide it alongside the glsl/core backends BUILD_SPIRV_TOOLING found)")
			endif()
		else()
			message(WARNING "BUILD_SPIRV_TOOLING is OFF or shaderc/spirv-cross weren't found — MetalRenderDevice will fail to compile any shader")
		endif()

		message(STATUS "Metal backend: Metal=${METAL_FRAMEWORK}")
	else()
		message(WARNING "Metal/QuartzCore/Foundation frameworks not found — Metal backend disabled.")
		if (PYROS_CONTEXT STREQUAL "SDL2Metal")
			message(FATAL_ERROR "SDL2Metal context was selected but the required frameworks are missing")
		endif()
	endif()
endif()
