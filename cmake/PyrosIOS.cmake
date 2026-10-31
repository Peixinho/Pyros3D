# Applied when configuring for iOS (CMAKE_SYSTEM_NAME=iOS - CMake sets IOS).
# The engine on an iPhone or the iOS Simulator: Metal, as on the Mac, with
# everything Homebrew gives the Mac build (SDL2, the shader tools) built
# here from source for the device, and the engine a static library inside
# the app.
#
#   cmake -S . -B build_ios_sim -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
#         -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 \
#         -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
#   (a device: -DCMAKE_OSX_SYSROOT=iphoneos and a signing team)

if (NOT IOS)
	return()
endif()

message(STATUS "Pyros3D: iOS - Metal + SDL2, the engine static, dependencies built from source")

set(PYROS_GRAPHICS "Metal" CACHE STRING "The GPU API the renderer draws with" FORCE)

set(STATIC_LIB ON CACHE BOOL "Build PyrosEngine as a static library" FORCE)
set(LIB_TYPE STATIC)

set(BUILD_VULKAN_BACKEND OFF CACHE BOOL "Build the Vulkan device" FORCE)
set(BUILD_METAL_BACKEND ON CACHE BOOL "Build the Metal device" FORCE)
set(BUILD_CONVERTER OFF CACHE BOOL "Build Assimp model converter tool" FORCE)
set(BUILD_EDITOR OFF CACHE BOOL "Build the editor" FORCE)
# (the player: an app that carries one built game - player/CMakeLists.txt, PYROS_IOS_GAME)
set(BUILD_PLAYER ON CACHE BOOL "Build the PyrosPlayer standalone game runtime" FORCE)
# (the app's files are in its bundle: looked up from where it runs, which MainProgram makes the bundle)
set(PYROS_RELOCATABLE_ASSETS ON CACHE BOOL "" FORCE)
set(PYROS_ASSET_ROOT "." CACHE STRING "" FORCE)

# (before any dependency is added: SDL2's sources are Objective-C, the engine's Metal ones Objective-C++)
enable_language(OBJC)
enable_language(OBJCXX)

include(FetchContent)
# (dependencies of older CMake - FreeType 2.13 asks for 2.8 - which CMake 4 no longer configures as they are)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# ---------------------------------------------------------------------------
# SDL2 (static) - the window, the Metal layer, touches
# ---------------------------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sdl2
	GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
	GIT_TAG release-2.32.10
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(sdl2)
if (NOT TARGET SDL2::SDL2)
	add_library(SDL2::SDL2 ALIAS SDL2-static)
endif()

# ---------------------------------------------------------------------------
# The shader tools: GLSL -> SPIR-V (glslang, the materials' surfaces) -> MSL
# (spirv-cross). The Mac build takes Homebrew's; the same versions here.
# ---------------------------------------------------------------------------
set(PYROS_KHRONOS_TAG vulkan-sdk-1.4.357.0)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_WERROR OFF CACHE BOOL "" FORCE)
set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ALLOW_EXTERNAL_SPIRV_TOOLS ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)

FetchContent_Declare(spirv_headers GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git GIT_TAG ${PYROS_KHRONOS_TAG} GIT_SHALLOW TRUE)
FetchContent_Declare(spirv_tools GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git GIT_TAG ${PYROS_KHRONOS_TAG} GIT_SHALLOW TRUE)
FetchContent_Declare(glslang GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git GIT_TAG ${PYROS_KHRONOS_TAG} GIT_SHALLOW TRUE)
FetchContent_Declare(spirv_cross GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git GIT_TAG ${PYROS_KHRONOS_TAG} GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(spirv_headers)
set(SPIRV-Headers_SOURCE_DIR ${spirv_headers_SOURCE_DIR} CACHE PATH "" FORCE)
FetchContent_MakeAvailable(spirv_tools glslang spirv_cross)

# What PyrosBackend.cmake's Metal section takes (in place of what it finds on the Mac)
# (the engine includes <spirv_cross/...>, as the headers are installed; the sources' folder is another name)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/pyros_include)
file(CREATE_LINK ${spirv_cross_SOURCE_DIR} ${CMAKE_BINARY_DIR}/pyros_include/spirv_cross SYMBOLIC)
set(PYROS_IOS_SPIRV_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/pyros_include)
set(SPIRV_CROSS_MSL_LIBRARY spirv-cross-msl CACHE STRING "" FORCE)
set(SPIRV_CROSS_GLSL_LIBRARY spirv-cross-glsl CACHE STRING "" FORCE)
set(SPIRV_CROSS_CORE_LIBRARY spirv-cross-core CACHE STRING "" FORCE)
