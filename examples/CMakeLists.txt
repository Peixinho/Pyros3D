cmake_minimum_required(VERSION 3.10)

project(Pyros3D_Examples)

set(CMAKE_CXX_STANDARD 17)

SET(EXAMPLES_PATH ${CMAKE_SOURCE_DIR}/examples)


# Check Build Type
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-D_DEBUG)
endif ()


# Find Freetype
find_package(Freetype REQUIRED)
if (NOT FREETYPE_FOUND)
	message(ERROR " Freetype2 not found!")
endif(NOT FREETYPE_FOUND)


# Find Bullet
find_package(Bullet REQUIRED)
if (NOT BULLET_FOUND)
	message(ERROR " Bullet not found!")
endif(NOT BULLET_FOUND)


# Desktop
option(IS_DESKTOP "Target is Desktop" ON)
if (IS_DESKTOP)
	SET(DESKTOP -DDESKTOP)
endif(IS_DESKTOP)


# LUA Support
if (HAVE_LUA_BINDINGS)
	SET(LUA_BINDINGS -DLUA_BINDINGS)
	add_definitions(-DLUA_BINDINGS)

	find_package(Lua51 REQUIRED)
	if (NOT LUA51_FOUND)
		message(ERROR " Lua not found!")
	endif(NOT LUA51_FOUND)
	set(LUA_LIB -llua)
endif(HAVE_LUA_BINDINGS)


# Set OpenGL Version
SET(OPENGL_VERSION "GL45" CACHE STRING "OpenGL Version")
if (OPENGL_VERSION STREQUAL "GL45")
	add_definitions(-DGL45)
	SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gl45/glad.c)
	# Find OpenGL
	find_package(OpenGL REQUIRED)
	if (NOT OPENGL_FOUND)
		message(ERROR " OpenGL not found!")
	endif(NOT OPENGL_FOUND)
	SET(OPENGL_LIBS ${OPENGL_LIBRARIES})
elseif (OPENGL_VERSION STREQUAL "GL41")
	add_definitions(-DGL45)
	SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gl41/glad.c)
	# Find OpenGL
	find_package(OpenGL REQUIRED)
	if (NOT OPENGL_FOUND)
		message(ERROR " OpenGL not found!")
	endif(NOT OPENGL_FOUND)
	SET(OPENGL_LIBS ${OPENGL_LIBRARIES})
else()
	SET(OPENGL_LIBS GLESv2)

	if (OPENGL_VERSION STREQUAL "GLES3")
		add_definitions(-DGLES3)
		if (IS_DESKTOP)
			SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gles3/glad.c)
		endif(IS_DESKTOP)
	endif()
endif()


# Context Library
set(CONTEXT "SDL2" CACHE STRING "SDL2/SDL2Vulkan/SFML/SDL")
if (CONTEXT STREQUAL "SDL2")

	# Find SDL2
	find_package(SDL2 REQUIRED)
	if (NOT SDL2_FOUND)
		message(ERROR " SDL2 not found!")
	endif(NOT SDL2_FOUND)
	set(WINDOW_MANAGER ${CMAKE_CURRENT_SOURCE_DIR}/WindowManagers/SDL2/SDL2Context.cpp)
	if (TARGET SDL2::SDL2)
		set(CONTEXT_LIB SDL2::SDL2)
	else()
		set(CONTEXT_LIB ${SDL2_LIBRARIES})
	endif()
	set(CONTEXT_DEFINITION -D_SDL2)

	elseif (CONTEXT STREQUAL "SDL2Vulkan")

		# Vulkan roadmap Phase 5 Step D - see VULKAN_ROADMAP.md. Needs the
		# main CMakeLists.txt's BUILD_VULKAN_BACKEND=ON (sets
		# VULKAN_BACKEND_FOUND, a plain - not cached - variable inherited
		# from the parent scope via add_subdirectory(), same as
		# BUILD_DEMOS/etc already relies on).
		if (NOT VULKAN_BACKEND_FOUND)
			message(FATAL_ERROR "CONTEXT=SDL2Vulkan requires -DBUILD_VULKAN_BACKEND=ON (and its dependencies - see CMakeLists.txt's BUILD_VULKAN_BACKEND option)")
		endif()
		find_package(SDL2 REQUIRED)
		if (NOT SDL2_FOUND)
			message(ERROR " SDL2 not found!")
		endif(NOT SDL2_FOUND)
		set(WINDOW_MANAGER ${CMAKE_CURRENT_SOURCE_DIR}/WindowManagers/SDL2Vulkan/SDL2VulkanContext.cpp)
		if (TARGET SDL2::SDL2)
			set(CONTEXT_LIB SDL2::SDL2 Vulkan::Vulkan volk::volk GPUOpen::VulkanMemoryAllocator)
		else()
			set(CONTEXT_LIB ${SDL2_LIBRARIES} Vulkan::Vulkan volk::volk GPUOpen::VulkanMemoryAllocator)
		endif()
		set(CONTEXT_DEFINITION -D_SDL2VULKAN)

	else()
		if (CONTEXT STREQUAL "SDL")

			# Find SDL
			find_package(SDL REQUIRED)
			if (NOT SDL2_FOUND)
				message(ERROR " SDL2 not found!")
			endif(NOT SDL2_FOUND)
			set(WINDOW_MANAGER ${CMAKE_CURRENT_SOURCE_DIR}/WindowManagers/SDL/SDLContext.cpp)
			set(CONTEXT_LIB ${SDL2_LIBRARIES})
			set(CONTEXT_DEFINITION -D_SDL2)

		else()
			if (CONTEXT STREQUAL "SFML")

				# Find SFML
				find_package(SFML REQUIRED)
				if (NOT SFML_FOUND)
					message(ERROR " SFML not found!")
				endif(NOT SFML_FOUND)
				set(WINDOW_MANAGER ${CMAKE_CURRENT_SOURCE_DIR}/WindowManagers/SFML/SFMLContext.cpp)
				set(CONTEXT_LIB ${SFML_LIBRARIES})
				set(CONTEXT_DEFINITION -D_SDL2)

			endif()
	endif()
endif()

# freebsd
link_directories("/usr/local/lib")

# Example Sources
set(SOURCE_RotatingCube
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingCube/RotatingCube.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_MSAATest
	${CMAKE_CURRENT_SOURCE_DIR}/MSAATest/MSAATest.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingCubeWithLighting
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingCubeWithLighting/RotatingCubeWithLighting.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingCubeWithLightingAndShadow
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingCubeWithLightingAndShadow/RotatingCubeWithLightingAndShadow.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingTexturedCube
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingTexturedCube/RotatingTexturedCube.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_PBRSpheres
	${CMAKE_CURRENT_SOURCE_DIR}/PBRSpheres/PBRSpheres.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SkyboxTest
	${CMAKE_CURRENT_SOURCE_DIR}/SkyboxTest/SkyboxTest.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingTextureAnimatedCube
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingTextureAnimatedCube/RotatingTextureAnimatedCube.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_CustomMaterial
	${CMAKE_CURRENT_SOURCE_DIR}/CustomMaterial/CustomMaterial.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_DepthOfField
	${CMAKE_CURRENT_SOURCE_DIR}/DepthOfField/DepthOfField.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_DeferredPBRSpheres
	${CMAKE_CURRENT_SOURCE_DIR}/DeferredPBRSpheres/DeferredPBRSpheres.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SSRTest
	${CMAKE_CURRENT_SOURCE_DIR}/SSRTest/SSRTest.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_DeferredRendering
	${CMAKE_CURRENT_SOURCE_DIR}/DeferredRendering/DeferredRendering.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_Decals
	${CMAKE_CURRENT_SOURCE_DIR}/Decals/Decals.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_IslandDemo
	${CMAKE_CURRENT_SOURCE_DIR}/IslandDemo/IslandDemo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_LOD_Example
	${CMAKE_CURRENT_SOURCE_DIR}/LOD_Example/LOD_Example.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_LightPriorityExample
	${CMAKE_CURRENT_SOURCE_DIR}/LightPriorityExample/LightPriorityExample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_LuaScripting
	${CMAKE_CURRENT_SOURCE_DIR}/LuaScripting/LuaScripting.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_NeonPulse
	${CMAKE_CURRENT_SOURCE_DIR}/NeonPulse/NeonPulse.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SceneSerializationExample
	${CMAKE_CURRENT_SOURCE_DIR}/SceneSerializationExample/SceneSerializationExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_ParallaxMapping
	${CMAKE_CURRENT_SOURCE_DIR}/ParallaxMapping/ParallaxMapping.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_ParticlesExample
	${CMAKE_CURRENT_SOURCE_DIR}/ParticlesExample/ParticlesExample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_PickingPainterMethod
	${CMAKE_CURRENT_SOURCE_DIR}/PickingPainterMethod/PickingPainterMethod.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RacingGame
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/RacingGame.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui_sdl2_context.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui/misc/freetype/imgui_freetype.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui_impl_sdl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui_impl_opengl3.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui/imgui_demo.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_ArenaFPS
	${CMAKE_CURRENT_SOURCE_DIR}/ArenaFPS/ArenaFPS.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SimplePhysics
	${CMAKE_CURRENT_SOURCE_DIR}/SimplePhysics/SimplePhysics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SkeletonAnimationExample
	${CMAKE_CURRENT_SOURCE_DIR}/SkeletonAnimationExample/SkeletonAnimationExample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SSAOExample
	${CMAKE_CURRENT_SOURCE_DIR}/SSAOExample/SSAOExample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_TextRendering
	${CMAKE_CURRENT_SOURCE_DIR}/TextRendering/TextRendering.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_MotionBlurExample
  ${CMAKE_CURRENT_SOURCE_DIR}/MotionBlurExample/MotionBlurExample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_ScreenSpaceReflection
  ${CMAKE_CURRENT_SOURCE_DIR}/ScreenSpaceReflection/ScreenSpaceReflection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
# Minimal repro harness for the effect-count-change black screen - see
# examples/EffectToggleTest/EffectToggleTest.h.
set(SOURCE_EffectToggleTest
	${CMAKE_CURRENT_SOURCE_DIR}/EffectToggleTest/EffectToggleTest.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)

set(SOURCE_DemoLauncher
	${CMAKE_CURRENT_SOURCE_DIR}/DemoLauncher/DemoLauncher.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)

# Headers
set(HEADERS
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}
	"/usr/local/include"
)
# Example Executable
add_executable(RotatingCube ${SOURCE_RotatingCube})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(RotatingCube PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(RotatingCube PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingCube -D_RotatingCube ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(MSAATest ${SOURCE_MSAATest})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(MSAATest PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(MSAATest PRIVATE ${DESKTOP} -DDEMO_NAME=MSAATest -D_MSAATest ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(RotatingCubeWithLighting ${SOURCE_RotatingCubeWithLighting})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(RotatingCubeWithLighting PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(RotatingCubeWithLighting PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingCubeWithLighting -D_RotatingCubeWithLighting ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(RotatingCubeWithLightingAndShadow ${SOURCE_RotatingCubeWithLightingAndShadow})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(RotatingCubeWithLightingAndShadow PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(RotatingCubeWithLightingAndShadow PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingCubeWithLightingAndShadow -D_RotatingCubeWithLightingAndShadow ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(RotatingTexturedCube ${SOURCE_RotatingTexturedCube})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(RotatingTexturedCube PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(RotatingTexturedCube PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingTexturedCube -D_RotatingTexturedCube ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(PBRSpheres ${SOURCE_PBRSpheres})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(PBRSpheres PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(PBRSpheres PRIVATE ${DESKTOP} -DDEMO_NAME=PBRSpheres -D_PBRSpheres ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(SkyboxTest ${SOURCE_SkyboxTest})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(SkyboxTest PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(SkyboxTest PRIVATE ${DESKTOP} -DDEMO_NAME=SkyboxTest -D_SkyboxTest ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(RotatingTextureAnimatedCube ${SOURCE_RotatingTextureAnimatedCube})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(RotatingTextureAnimatedCube PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(RotatingTextureAnimatedCube PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingTextureAnimatedCube -D_RotatingTextureAnimatedCube ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(CustomMaterial ${SOURCE_CustomMaterial})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(CustomMaterial PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(CustomMaterial PRIVATE ${DESKTOP} -DDEMO_NAME=CustomMaterial -D_CustomMaterial ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(DepthOfField ${SOURCE_DepthOfField})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(DepthOfField PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(DepthOfField PRIVATE ${DESKTOP} -DDEMO_NAME=DepthOfField -D_DepthOfField ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(DeferredPBRSpheres ${SOURCE_DeferredPBRSpheres})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(DeferredPBRSpheres PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(DeferredPBRSpheres PRIVATE ${DESKTOP} -DDEMO_NAME=DeferredPBRSpheres -D_DeferredPBRSpheres ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(SSRTest ${SOURCE_SSRTest})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(SSRTest PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(SSRTest PRIVATE ${DESKTOP} -DDEMO_NAME=SSRTest -D_SSRTest ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(DeferredRendering ${SOURCE_DeferredRendering})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(DeferredRendering PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(DeferredRendering PRIVATE ${DESKTOP} -DDEMO_NAME=DeferredRendering -D_DeferredRendering ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(Decals ${SOURCE_Decals})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(Decals PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(Decals PRIVATE ${DESKTOP} -DDEMO_NAME=Decals -D_Decals ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(IslandDemo ${SOURCE_IslandDemo})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(IslandDemo PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(IslandDemo PRIVATE ${DESKTOP} -DDEMO_NAME=IslandDemo -D_IslandDemo ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(LOD_Example ${SOURCE_LOD_Example})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(LOD_Example PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(LOD_Example PRIVATE ${DESKTOP} -DDEMO_NAME=LOD_Example -D_LOD_Example ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(LightPriorityExample ${SOURCE_LightPriorityExample})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(LightPriorityExample PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(LightPriorityExample PRIVATE ${DESKTOP} -DDEMO_NAME=LightPriorityExample -D_LightPriorityExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

if (HAVE_LUA_BINDINGS)
add_executable(LuaScripting ${SOURCE_LuaScripting})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(LuaScripting PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(LuaScripting PRIVATE ${DESKTOP} -DDEMO_NAME=LuaScripting -D_LuaScripting ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(NeonPulse ${SOURCE_NeonPulse})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(NeonPulse PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(NeonPulse PRIVATE ${DESKTOP} -DDEMO_NAME=NeonPulse -D_NeonPulse ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})
endif()

add_executable(SceneSerializationExample ${SOURCE_SceneSerializationExample})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(SceneSerializationExample PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(SceneSerializationExample PRIVATE ${DESKTOP} -DDEMO_NAME=SceneSerializationExample -D_SceneSerializationExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(ParallaxMapping ${SOURCE_ParallaxMapping})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(ParallaxMapping PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(ParallaxMapping PRIVATE ${DESKTOP} -DDEMO_NAME=ParallaxMapping -D_ParallaxMapping ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(ParticlesExample ${SOURCE_ParticlesExample})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(ParticlesExample PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(ParticlesExample PRIVATE ${DESKTOP} -DDEMO_NAME=ParticlesExample -D_ParticlesExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(PickingPainterMethod ${SOURCE_PickingPainterMethod})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(PickingPainterMethod PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(PickingPainterMethod PRIVATE ${DESKTOP} -DDEMO_NAME=PickingPainterMethod -D_PickingPainterMethod ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(SkeletonAnimationExample ${SOURCE_SkeletonAnimationExample})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(SkeletonAnimationExample PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(SkeletonAnimationExample PRIVATE ${DESKTOP} -DDEMO_NAME=SkeletonAnimationExample -D_SkeletonAnimationExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

#add_executable(RacingGame ${SOURCE_RacingGame})
#include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui)
#target_link_libraries(RacingGame PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
#target_compile_definitions(RacingGame PRIVATE ${DESKTOP} -DDEMO_NAME=RacingGame -D_RacingGame ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(SimplePhysics ${SOURCE_SimplePhysics})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(SimplePhysics PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(SimplePhysics PRIVATE ${DESKTOP} -DDEMO_NAME=SimplePhysics -D_SimplePhysics ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(ArenaFPS ${SOURCE_ArenaFPS})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(ArenaFPS PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(ArenaFPS PRIVATE ${DESKTOP} -DDEMO_NAME=ArenaFPS -D_ArenaFPS ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(SSAOExample ${SOURCE_SSAOExample})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(SSAOExample PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(SSAOExample PRIVATE ${DESKTOP} -DDEMO_NAME=SSAOExample -D_SSAOExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(TextRendering ${SOURCE_TextRendering})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(TextRendering PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(TextRendering PRIVATE ${DESKTOP} -DDEMO_NAME=TextRendering -D_TextRendering ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(MotionBlurExample ${SOURCE_MotionBlurExample})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(MotionBlurExample PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(MotionBlurExample PRIVATE ${DESKTOP} -DDEMO_NAME=MotionBlurExample -D_MotionBlurExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(ScreenSpaceReflection ${SOURCE_ScreenSpaceReflection})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(ScreenSpaceReflection PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(ScreenSpaceReflection PRIVATE ${DESKTOP} -DDEMO_NAME=ScreenSpaceReflection -D_ScreenSpaceReflection ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

# Unified demo browser - lists every converted demo (scene JSON + attached
# Lua behavior) in one process, see examples/DemoLauncher/DemoLauncher.h.
# Requires real Lua bindings (sol::state, GenerateBindings, LuaComponent
# used unconditionally, no #ifdef LUA_BINDINGS guards) - same requirement
# LuaScripting has above.
if (HAVE_LUA_BINDINGS)
add_executable(EffectToggleTest ${SOURCE_EffectToggleTest})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(EffectToggleTest PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(EffectToggleTest PRIVATE ${DESKTOP} -DDEMO_NAME=EffectToggleTest -D_EffectToggleTest ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(DemoLauncher ${SOURCE_DemoLauncher})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})
target_link_libraries(DemoLauncher PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(DemoLauncher PRIVATE ${DESKTOP} -DDEMO_NAME=DemoLauncher -D_DemoLauncher ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})
endif()

# symlink, not file(COPY): a copy goes stale the moment resources/shaders/*.glsl
# changes, since this line only re-runs on a cmake reconfigure, not on every
# build - real source of a very confusing "edited the shader but nothing
# changed" session. A symlink always reflects the live source tree.
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/shaders)
	file(CREATE_LINK ${CMAKE_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders SYMBOLIC)
endif()
