cmake_minimum_required(VERSION 3.1)

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
else()
		SET(OPENGL_LIBS GLESv2)

		if (OPENGL_VERSION STREQUAL "GLES2")
			add_definitions(-DGLES2)
			if (IS_DESKTOP)
				SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gles2/glad.c)
			endif(IS_DESKTOP)
		else()
		if (OPENGL_VERSION STREQUAL "GLES3")
			add_definitions(-DGLES3)
			if (IS_DESKTOP)
				SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gles3/glad.c)
			endif(IS_DESKTOP)
		endif()
	endif()
endif()


# Context Library
set(CONTEXT "SDL2" CACHE STRING "SDL2/SFML/SDL")
if (CONTEXT STREQUAL "SDL2")

	# Find SDL2
	find_package(SDL2 REQUIRED)
	if (NOT SDL2_FOUND)
		message(ERROR " SDL2 not found!")
	endif(NOT SDL2_FOUND)
	set(WINDOW_MANAGER ${CMAKE_CURRENT_SOURCE_DIR}/WindowManagers/SDL2/SDL2Context.cpp)
	#set(CONTEXT_LIB ${SDL2_LIBRARIES})
	set(CONTEXT_LIB -lSDL2)
	set(CONTEXT_DEFINITION -D_SDL2)

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
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingCubeWithLighting
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingCubeWithLighting/RotatingCubeWithLighting.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingCubeWithLightingAndShadow
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingCubeWithLightingAndShadow/RotatingCubeWithLightingAndShadow.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingTexturedCube
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingTexturedCube/RotatingTexturedCube.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingTextureAnimatedCube
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingTextureAnimatedCube/RotatingTextureAnimatedCube.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_CustomMaterial
	${CMAKE_CURRENT_SOURCE_DIR}/CustomMaterial/CustomMaterial.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_DepthOfField
	${CMAKE_CURRENT_SOURCE_DIR}/DepthOfField/DepthOfField.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_DeferredRendering
	${CMAKE_CURRENT_SOURCE_DIR}/DeferredRendering/DeferredRendering.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_Decals
	${CMAKE_CURRENT_SOURCE_DIR}/Decals/Decals.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_IslandDemo
	${CMAKE_CURRENT_SOURCE_DIR}/IslandDemo/IslandDemo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_LOD_Example
	${CMAKE_CURRENT_SOURCE_DIR}/LOD_Example/LOD_Example.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_LuaScripting
	${CMAKE_CURRENT_SOURCE_DIR}/LuaScripting/LuaScripting.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_ParallaxMapping
	${CMAKE_CURRENT_SOURCE_DIR}/ParallaxMapping/ParallaxMapping.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_ParticlesExample
	${CMAKE_CURRENT_SOURCE_DIR}/ParticlesExample/ParticlesExample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_PickingPainterMethod
	${CMAKE_CURRENT_SOURCE_DIR}/PickingPainterMethod/PickingPainterMethod.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RacingGame
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/RacingGame.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui_sdl2_context.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui/imgui_draw.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui/imgui.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui/misc/freetype/imgui_freetype.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui_impl_sdl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui_impl_opengl3.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui/imgui_demo.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SimplePhysics
	${CMAKE_CURRENT_SOURCE_DIR}/SimplePhysics/SimplePhysics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SkeletonAnimationExample
	${CMAKE_CURRENT_SOURCE_DIR}/SkeletonAnimationExample/SkeletonAnimationExample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SSAOExample
	${CMAKE_CURRENT_SOURCE_DIR}/SSAOExample/SSAOExample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_TextRendering
	${CMAKE_CURRENT_SOURCE_DIR}/TextRendering/TextRendering.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_MotionBlurExample
  ${CMAKE_CURRENT_SOURCE_DIR}/MotionBlurExample/MotionBlurExample.cpp
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
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingCube PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(RotatingCube PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingCube -D_RotatingCube ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(RotatingCubeWithLighting ${SOURCE_RotatingCubeWithLighting})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingCubeWithLighting PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(RotatingCubeWithLighting PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingCubeWithLighting -D_RotatingCubeWithLighting ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(RotatingCubeWithLightingAndShadow ${SOURCE_RotatingCubeWithLightingAndShadow})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingCubeWithLightingAndShadow PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(RotatingCubeWithLightingAndShadow PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingCubeWithLightingAndShadow -D_RotatingCubeWithLightingAndShadow ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(RotatingTexturedCube ${SOURCE_RotatingTexturedCube})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingTexturedCube PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(RotatingTexturedCube PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingTexturedCube -D_RotatingTexturedCube ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(RotatingTextureAnimatedCube ${SOURCE_RotatingTextureAnimatedCube})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingTextureAnimatedCube PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(RotatingTextureAnimatedCube PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingTextureAnimatedCube -D_RotatingTextureAnimatedCube ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(CustomMaterial ${SOURCE_CustomMaterial})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(CustomMaterial PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(CustomMaterial PRIVATE ${DESKTOP} -DDEMO_NAME=CustomMaterial -D_CustomMaterial ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(DepthOfField ${SOURCE_DepthOfField})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(DepthOfField PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(DepthOfField PRIVATE ${DESKTOP} -DDEMO_NAME=DepthOfField -D_DepthOfField ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(DeferredRendering ${SOURCE_DeferredRendering})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(DeferredRendering PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(DeferredRendering PRIVATE ${DESKTOP} -DDEMO_NAME=DeferredRendering -D_DeferredRendering ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(Decals ${SOURCE_Decals})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(Decals PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(Decals PRIVATE ${DESKTOP} -DDEMO_NAME=Decals -D_Decals ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(IslandDemo ${SOURCE_IslandDemo})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(IslandDemo PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(IslandDemo PRIVATE ${DESKTOP} -DDEMO_NAME=IslandDemo -D_IslandDemo ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(LOD_Example ${SOURCE_LOD_Example})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(LOD_Example PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(LOD_Example PRIVATE ${DESKTOP} -DDEMO_NAME=LOD_Example -D_LOD_Example ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

if (HAVE_LUA_BINDINGS)
add_executable(LuaScripting ${SOURCE_LuaScripting})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(LuaScripting PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(LuaScripting PRIVATE ${DESKTOP} -DDEMO_NAME=LuaScripting -D_LuaScripting ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})
endif()

add_executable(ParallaxMapping ${SOURCE_ParallaxMapping})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(ParallaxMapping PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(ParallaxMapping PRIVATE ${DESKTOP} -DDEMO_NAME=ParallaxMapping -D_ParallaxMapping ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(ParticlesExample ${SOURCE_ParticlesExample})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(ParticlesExample PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(ParticlesExample PRIVATE ${DESKTOP} -DDEMO_NAME=ParticlesExample -D_ParticlesExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(PickingPainterMethod ${SOURCE_PickingPainterMethod})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(PickingPainterMethod PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(PickingPainterMethod PRIVATE ${DESKTOP} -DDEMO_NAME=PickingPainterMethod -D_PickingPainterMethod ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(SkeletonAnimationExample ${SOURCE_SkeletonAnimationExample})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(SkeletonAnimationExample PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(SkeletonAnimationExample PRIVATE ${DESKTOP} -DDEMO_NAME=SkeletonAnimationExample -D_SkeletonAnimationExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

#add_executable(RacingGame ${SOURCE_RacingGame})
#include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/imgui)
#target_link_libraries(RacingGame PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
#target_compile_definitions(RacingGame PRIVATE ${DESKTOP} -DDEMO_NAME=RacingGame -D_RacingGame ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(SimplePhysics ${SOURCE_SimplePhysics})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(SimplePhysics PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(SimplePhysics PRIVATE ${DESKTOP} -DDEMO_NAME=SimplePhysics -D_SimplePhysics ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(SSAOExample ${SOURCE_SSAOExample})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(SSAOExample PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(SSAOExample PRIVATE ${DESKTOP} -DDEMO_NAME=SSAOExample -D_SSAOExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(TextRendering ${SOURCE_TextRendering})
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(TextRendering PyrosEngine  ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(TextRendering PRIVATE ${DESKTOP} -DDEMO_NAME=TextRendering -D_TextRendering ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

add_executable(MotionBlurExample ${SOURCE_MotionBlurExample})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(MotionBlurExample PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES} pthread dl)
target_compile_definitions(MotionBlurExample PRIVATE ${DESKTOP} -DDEMO_NAME=MotionBlurExample -D_MotionBlurExample ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

file(COPY ${CMAKE_SOURCE_DIR}/resources/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
