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
	set(OPENGL_LIBS GLESv2)
	if (IS_DESKTOP)
		set(GL_INCLUDE ${PYROS_EXT_DIR}/gles3/glad.c)
		set(GLAD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/Pyros3D/Ext/gles3)
	endif()
else()
	message(FATAL_ERROR "Unknown OPENGL_VERSION='${OPENGL_VERSION}' (expected GL45, GL42, GL41, or GLES3)")
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
	find_library(SHADERC_LIBRARY NAMES shaderc_combined PATHS ${HOMEBREW_SHADERC_PREFIX}/lib)
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
		message(STATUS "SPIR-V tooling: shaderc=${SHADERC_LIBRARY}")
	else()
		message(WARNING "shaderc/spirv-cross not found — SPIR-V tooling disabled. Install: brew install shaderc spirv-cross")
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
