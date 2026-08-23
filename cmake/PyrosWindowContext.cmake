# Resolve window manager + ImGui backend from PYROS_CONTEXT.
# Used by examples/ (and anything else that needs a window).
# Expects: PYROS_CONTEXT, VULKAN_BACKEND_FOUND, IMGUI_DIR

set(PYROS_WINDOW_MANAGER "")
set(PYROS_CONTEXT_LIB "")
set(PYROS_CONTEXT_DEFINITION "")
set(PYROS_IMGUI_BACKEND_SOURCE "")

if (PYROS_CONTEXT STREQUAL "SDL2")
	if (EMSCRIPTEN)
		# SDL2 comes from -sUSE_SDL=2 (PyrosEmscripten.cmake); no find_package.
		if (NOT TARGET SDL2::SDL2)
			add_library(SDL2::SDL2 INTERFACE IMPORTED)
		endif()
		set(PYROS_CONTEXT_LIB SDL2::SDL2)
		set(SDL2_INCLUDE_DIRS "")
	elseif (ANDROID)
		# Resolved in PyrosDependencies.cmake (required for File.cpp too).
		set(PYROS_CONTEXT_LIB SDL2::SDL2)
		set(SDL2_INCLUDE_DIRS "")
	else()
		find_package(SDL2 REQUIRED)
		if (TARGET SDL2::SDL2)
			set(PYROS_CONTEXT_LIB SDL2::SDL2)
		else()
			set(PYROS_CONTEXT_LIB ${SDL2_LIBRARIES})
		endif()
	endif()
	set(PYROS_WINDOW_MANAGER ${CMAKE_SOURCE_DIR}/examples/WindowManagers/SDL2/SDL2Context.cpp)
	set(PYROS_CONTEXT_DEFINITION _SDL2)
	set(PYROS_IMGUI_BACKEND_SOURCE
		${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
		${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	)

elseif (PYROS_CONTEXT STREQUAL "SDL2Vulkan")
	if (NOT VULKAN_BACKEND_FOUND)
		message(FATAL_ERROR "SDL2Vulkan requires a successful BUILD_VULKAN_BACKEND (Vulkan SDK + volk + VMA)")
	endif()
	find_package(SDL2 REQUIRED)
	set(PYROS_WINDOW_MANAGER ${CMAKE_SOURCE_DIR}/examples/WindowManagers/SDL2Vulkan/SDL2VulkanContext.cpp)
	if (TARGET SDL2::SDL2)
		set(PYROS_CONTEXT_LIB SDL2::SDL2 Vulkan::Vulkan volk::volk GPUOpen::VulkanMemoryAllocator)
	else()
		set(PYROS_CONTEXT_LIB ${SDL2_LIBRARIES} Vulkan::Vulkan volk::volk GPUOpen::VulkanMemoryAllocator)
	endif()
	set(PYROS_CONTEXT_DEFINITION _SDL2VULKAN)
	# OpenGL ImGui backend must stay out of Vulkan builds.
	set(PYROS_IMGUI_BACKEND_SOURCE ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp)

elseif (PYROS_CONTEXT STREQUAL "SDL2Metal")
	if (NOT METAL_BACKEND_FOUND)
		message(FATAL_ERROR "SDL2Metal requires a successful BUILD_METAL_BACKEND (Metal/QuartzCore/Foundation frameworks, Apple only)")
	endif()
	find_package(SDL2 REQUIRED)
	set(PYROS_WINDOW_MANAGER ${CMAKE_SOURCE_DIR}/examples/WindowManagers/SDL2Metal/SDL2MetalContext.cpp)
	if (TARGET SDL2::SDL2)
		set(PYROS_CONTEXT_LIB SDL2::SDL2 ${METAL_BACKEND_LIBS})
	else()
		set(PYROS_CONTEXT_LIB ${SDL2_LIBRARIES} ${METAL_BACKEND_LIBS})
	endif()
	set(PYROS_CONTEXT_DEFINITION _SDL2METAL)
	# imgui_impl_sdl2.cpp only, same as SDL2Vulkan - SDL2MetalContext::
	# GetEvents() calls ImGui_ImplSDL2_ProcessEvent() unconditionally
	# (build-time symbol, even though the runtime call is guarded), so
	# this must always be linked in regardless of whether ImGui is ever
	# actually initialized. No real ImGui-on-Metal *rendering* backend
	# wired in yet (imgui_impl_opengl3.cpp is deliberately excluded, same
	# as Vulkan) - matches SDL2VulkanContext's own bring-up order (window +
	# device + clear/present came first, ImGui came later - see
	# VulkanImGuiBackend.cpp's history); BaseExample::InitImGui() leaves
	# imguiInitialized false on _SDL2METAL, which already makes every
	# other ImGui* method a no-op - see its comment. imgui_impl_metal.mm
	# already exists vendored under src/Pyros3D/Ext/imgui/backends if/when
	# that's the next step.
	set(PYROS_IMGUI_BACKEND_SOURCE ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp)

elseif (PYROS_CONTEXT STREQUAL "SDL")
	find_package(SDL REQUIRED)
	set(PYROS_WINDOW_MANAGER ${CMAKE_SOURCE_DIR}/examples/WindowManagers/SDL/SDLContext.cpp)
	set(PYROS_CONTEXT_LIB ${SDL_LIBRARY})
	set(PYROS_CONTEXT_DEFINITION _SDL2)
	set(PYROS_IMGUI_BACKEND_SOURCE
		${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
		${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	)

elseif (PYROS_CONTEXT STREQUAL "SFML")
	find_package(SFML REQUIRED)
	set(PYROS_WINDOW_MANAGER ${CMAKE_SOURCE_DIR}/examples/WindowManagers/SFML/SFMLContext.cpp)
	set(PYROS_CONTEXT_LIB ${SFML_LIBRARIES})
	set(PYROS_CONTEXT_DEFINITION _SFML)
	set(PYROS_IMGUI_BACKEND_SOURCE
		${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
		${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	)

else()
	message(FATAL_ERROR "Unknown PYROS_CONTEXT='${PYROS_CONTEXT}' (expected SDL2, SDL2Vulkan, SDL2Metal, SDL, or SFML)")
endif()

# On Windows, SDL_main.h does `#define main SDL_main`, so the application's
# int main() is compiled under a different name and the real entry point has
# to come from SDL2main - otherwise the link fails with LNK2019 "unresolved
# external symbol main referenced in __scrt_common_main_seh". It has to
# precede SDL2 in link order, hence INSERT rather than APPEND. Covers every
# SDL2-based context above; the TARGET guard skips the legacy SDL/SFML ones.
if (WIN32 AND TARGET SDL2::SDL2main)
	list(INSERT PYROS_CONTEXT_LIB 0 SDL2::SDL2main)
	message(STATUS "  SDL2main            = linked (Windows entry point)")
endif()

message(STATUS "  Window manager      = ${PYROS_CONTEXT} → ${PYROS_WINDOW_MANAGER}")
