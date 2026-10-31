# The window: SDL2's (examples/WindowManagers/SDL2), with the device the renderer draws with on it.
# Used by examples/, editor/ and player/. Expects: IMGUI_DIR, VULKAN_BACKEND_LIBS, METAL_BACKEND_LIBS

set(PYROS_WINDOW_MANAGER ${CMAKE_SOURCE_DIR}/examples/WindowManagers/SDL2/SDL2Context.cpp)
set(PYROS_CONTEXT_DEFINITION _SDL2)
set(PYROS_IMGUI_BACKEND_SOURCE ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp)

if (EMSCRIPTEN)
	# SDL2 comes from -sUSE_SDL=2 (PyrosEmscripten.cmake); no find_package.
	if (NOT TARGET SDL2::SDL2)
		add_library(SDL2::SDL2 INTERFACE IMPORTED)
	endif()
	set(SDL2_INCLUDE_DIRS "")
elseif (ANDROID OR IOS)
	# Resolved in PyrosDependencies.cmake / PyrosIOS.cmake.
	set(SDL2_INCLUDE_DIRS "")
elseif (NOT TARGET SDL2::SDL2)
	find_package(SDL2 REQUIRED)
endif()
if (TARGET SDL2::SDL2)
	set(PYROS_CONTEXT_LIB SDL2::SDL2)
else()
	set(PYROS_CONTEXT_LIB ${SDL2_LIBRARIES})
endif()
# (vulkan-1 stays off the link line wherever volk is used - see PyrosBackend.cmake)
list(APPEND PYROS_CONTEXT_LIB ${VULKAN_BACKEND_LIBS} ${METAL_BACKEND_LIBS})

# On Windows, SDL_main.h does `#define main SDL_main`, so the application's
# int main() is compiled under a different name and the real entry point has
# to come from SDL2main - otherwise the link fails with LNK2019 "unresolved
# external symbol main referenced in __scrt_common_main_seh". It has to
# precede SDL2 in link order, hence INSERT rather than APPEND.
if (WIN32 AND TARGET SDL2::SDL2main)
	list(INSERT PYROS_CONTEXT_LIB 0 SDL2::SDL2main)
	message(STATUS "  SDL2main            = linked (Windows entry point)")
endif()

message(STATUS "  Window manager      = SDL2 → ${PYROS_WINDOW_MANAGER}")
