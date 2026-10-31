# The devices' dependencies (the renderer's own setup: PyrosRenderer.cmake) and ImGui. Expects options + deps loaded.

set(PYROS_EXT_DIR ${CMAKE_SOURCE_DIR}/src/Pyros3D/Ext)

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
# Vulkan (Linux, Windows; MoltenVK on a Mac): volk loads it, VMA allocates
# ---------------------------------------------------------------------------
set(VULKAN_BACKEND_FOUND OFF)
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
		# Vulkan::Headers, NOT Vulkan::Vulkan. volk dlopen's the loader
		# itself and owns the vk* symbols as writable globals; linking
		# vulkan-1 alongside it puts the loader's import thunks in the
		# binary under the same names, which volk's README calls out
		# directly ("it's also important to make sure that vulkan-1 is not
		# linked into the application, as this results in symbol name
		# conflicts"). On Windows the thunk wins, so volkInitialize()'s
		# `vkGetInstanceProcAddr = ...` writes into .text and takes an
		# access violation on the very first Vulkan call.
		if (TARGET Vulkan::Headers)
			set(VULKAN_BACKEND_LIBS Vulkan::Headers volk::volk GPUOpen::VulkanMemoryAllocator)
		else()
			# CMake < 3.21 has no Headers target; use the include dir alone
			# and still keep the loader off the link line.
			set(VULKAN_BACKEND_LIBS volk::volk GPUOpen::VulkanMemoryAllocator)
			set(PYROS_DEVICE_INCLUDE_DIRS ${PYROS_DEVICE_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS})
		endif()
		message(STATUS "Vulkan: Vulkan=${Vulkan_LIBRARY}, volk+VMA via FetchContent")
	else()
		message(WARNING "Vulkan SDK not found - the Vulkan device is off. Install the LunarG SDK or: brew install vulkan-headers vulkan-loader")
	endif()
endif()

# ---------------------------------------------------------------------------
# Metal (Apple): its shaders are the engine's SPIR-V, turned into MSL by
# spirv-cross when a pipeline is made
# ---------------------------------------------------------------------------
set(METAL_BACKEND_FOUND OFF)
set(METAL_BACKEND_LIBS "")

if (BUILD_METAL_BACKEND AND APPLE)
	find_library(METAL_FRAMEWORK Metal)
	find_library(QUARTZCORE_FRAMEWORK QuartzCore)
	find_library(FOUNDATION_FRAMEWORK Foundation)
	find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
	if (NOT IOS)
		execute_process(COMMAND brew --prefix spirv-cross
			OUTPUT_VARIABLE HOMEBREW_SPIRV_CROSS_PREFIX
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
	endif()
	find_path(SPIRV_CROSS_INCLUDE_DIR spirv_cross/spirv_cross.hpp HINTS ${PYROS_IOS_SPIRV_INCLUDE_DIRS} ${HOMEBREW_SPIRV_CROSS_PREFIX}/include)
	find_library(SPIRV_CROSS_MSL_LIBRARY NAMES spirv-cross-msl HINTS ${HOMEBREW_SPIRV_CROSS_PREFIX}/lib)
	find_library(SPIRV_CROSS_GLSL_LIBRARY NAMES spirv-cross-glsl HINTS ${HOMEBREW_SPIRV_CROSS_PREFIX}/lib)
	find_library(SPIRV_CROSS_CORE_LIBRARY NAMES spirv-cross-core HINTS ${HOMEBREW_SPIRV_CROSS_PREFIX}/lib)

	if (METAL_FRAMEWORK AND QUARTZCORE_FRAMEWORK AND FOUNDATION_FRAMEWORK AND SPIRV_CROSS_INCLUDE_DIR AND SPIRV_CROSS_MSL_LIBRARY)
		enable_language(OBJCXX)
		set(METAL_BACKEND_FOUND ON)
		# By name, not by the absolute paths found above - see
		# pyros_frameworks_by_name() on why an absolute .framework on the
		# link line drags every other framework into the same SDK.
		set(METAL_BACKEND_LIBS ${METAL_FRAMEWORK} ${QUARTZCORE_FRAMEWORK}
			${FOUNDATION_FRAMEWORK} ${COREGRAPHICS_FRAMEWORK})
		pyros_frameworks_by_name(METAL_BACKEND_LIBS)
		list(APPEND METAL_BACKEND_LIBS ${SPIRV_CROSS_MSL_LIBRARY} ${SPIRV_CROSS_GLSL_LIBRARY} ${SPIRV_CROSS_CORE_LIBRARY})
		set(PYROS_DEVICE_INCLUDE_DIRS ${PYROS_DEVICE_INCLUDE_DIRS} ${SPIRV_CROSS_INCLUDE_DIR})
		message(STATUS "Metal: Metal=${METAL_FRAMEWORK}, spirv-cross-msl=${SPIRV_CROSS_MSL_LIBRARY}")
	else()
		message(WARNING "Metal frameworks or spirv-cross (brew install spirv-cross) not found - the Metal device is off.")
	endif()
endif()
