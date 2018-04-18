cmake_minimum_required(VERSION 3.1)

project(Pyros3D_Examples)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

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
	set(CONTEXT_LIB ${SDL2_LIBRARIES})
	set(CONTEXT_DEFINITION -D_SDL2)

	else()
		if (CONTEXT STREQUAL "SDL")

			# Find SDL
			find_package(SDL REQUIRED)
			if (NOT SDL_FOUND)
				message(ERROR " SDL not found!")
			endif(NOT SDL_FOUND)
			set(WINDOW_MANAGER ${CMAKE_CURRENT_SOURCE_DIR}/WindowManagers/SDL/SDLContext.cpp)
			set(CONTEXT_LIB ${SDL_LIBRARIES})
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


# Example Sources
set(SOURCE_RotatingCube
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingCube/RotatingCube.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingCubeWithLighting
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingCubeWithLighting/RotatingCubeWithLighting.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingCubeWithLightingAndShadow
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingCubeWithLightingAndShadow/RotatingCubeWithLightingAndShadow.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingTexturedCube
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingTexturedCube/RotatingTexturedCube.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RotatingTextureAnimatedCube
	${CMAKE_CURRENT_SOURCE_DIR}/RotatingTextureAnimatedCube/RotatingTextureAnimatedCube.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_CustomMaterial
	${CMAKE_CURRENT_SOURCE_DIR}/CustomMaterial/CustomMaterial.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_DepthOfField
	${CMAKE_CURRENT_SOURCE_DIR}/DepthOfField/DepthOfField.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_DeferredRendering
	${CMAKE_CURRENT_SOURCE_DIR}/DeferredRendering/DeferredRendering.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_Decals
	${CMAKE_CURRENT_SOURCE_DIR}/Decals/Decals.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_ImGuiExample
	${CMAKE_CURRENT_SOURCE_DIR}/ImGuiExample/ImGuiExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_IslandDemo
	${CMAKE_CURRENT_SOURCE_DIR}/IslandDemo/IslandDemo.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_LOD_Example
	${CMAKE_CURRENT_SOURCE_DIR}/LOD_Example/LOD_Example.cpp
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
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_PickingPainterMethod
	${CMAKE_CURRENT_SOURCE_DIR}/PickingPainterMethod/PickingPainterMethod.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_RacingGame
	${CMAKE_CURRENT_SOURCE_DIR}/RacingGame/RacingGame.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SimplePhysics
	${CMAKE_CURRENT_SOURCE_DIR}/SimplePhysics/SimplePhysics.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SkeletonAnimationExample
	${CMAKE_CURRENT_SOURCE_DIR}/SkeletonAnimationExample/SkeletonAnimationExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_SSAOExample
	${CMAKE_CURRENT_SOURCE_DIR}/SSAOExample/SSAOExample.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
set(SOURCE_TextRendering
	${CMAKE_CURRENT_SOURCE_DIR}/TextRendering/TextRendering.cpp
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)

# Headers
set(HEADERS
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}
)

# Example Executable
add_executable(RotatingCube ${SOURCE_RotatingCube})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingCube PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(RotatingCube PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingCube -D_RotatingCube ${CONTEXT_DEFINITION})

add_executable(RotatingCubeWithLighting ${SOURCE_RotatingCubeWithLighting})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingCubeWithLighting PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(RotatingCubeWithLighting PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingCubeWithLighting -D_RotatingCubeWithLighting ${CONTEXT_DEFINITION})

add_executable(RotatingCubeWithLightingAndShadow ${SOURCE_RotatingCubeWithLightingAndShadow})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingCubeWithLightingAndShadow PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(RotatingCubeWithLightingAndShadow PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingCubeWithLightingAndShadow -D_RotatingCubeWithLightingAndShadow ${CONTEXT_DEFINITION})

add_executable(RotatingTexturedCube ${SOURCE_RotatingTexturedCube})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingTexturedCube PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(RotatingTexturedCube PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingTexturedCube -D_RotatingTexturedCube ${CONTEXT_DEFINITION})

add_executable(RotatingTextureAnimatedCube ${SOURCE_RotatingTextureAnimatedCube})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(RotatingTextureAnimatedCube PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(RotatingTextureAnimatedCube PRIVATE ${DESKTOP} -DDEMO_NAME=RotatingTextureAnimatedCube -D_RotatingTextureAnimatedCube ${CONTEXT_DEFINITION})

add_executable(CustomMaterial ${SOURCE_CustomMaterial})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(CustomMaterial PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(CustomMaterial PRIVATE ${DESKTOP} -DDEMO_NAME=CustomMaterial -D_CustomMaterial ${CONTEXT_DEFINITION})

add_executable(DepthOfField ${SOURCE_DepthOfField})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(DepthOfField PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(DepthOfField PRIVATE ${DESKTOP} -DDEMO_NAME=DepthOfField -D_DepthOfField ${CONTEXT_DEFINITION})

add_executable(DeferredRendering ${SOURCE_DeferredRendering})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(DeferredRendering PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(DeferredRendering PRIVATE ${DESKTOP} -DDEMO_NAME=DeferredRendering -D_DeferredRendering ${CONTEXT_DEFINITION})

add_executable(Decals ${SOURCE_Decals})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(Decals PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(Decals PRIVATE ${DESKTOP} -DDEMO_NAME=Decals -D_Decals ${CONTEXT_DEFINITION})

#add_executable(ImGuiExample ${SOURCE_ImGuiExample})
#include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
#target_link_libraries(ImGuiExample PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
#target_compile_definitions(ImGuiExample PRIVATE ${DESKTOP} -DDEMO_NAME=ImGuiExample -D_ImGuiExample ${CONTEXT_DEFINITION})

add_executable(IslandDemo ${SOURCE_IslandDemo})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(IslandDemo PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(IslandDemo PRIVATE ${DESKTOP} -DDEMO_NAME=IslandDemo -D_IslandDemo ${CONTEXT_DEFINITION})

add_executable(LOD_Example ${SOURCE_LOD_Example})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(LOD_Example PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(LOD_Example PRIVATE ${DESKTOP} -DDEMO_NAME=LOD_Example -D_LOD_Example ${CONTEXT_DEFINITION})

#add_executable(LuaScripting ${SOURCE_LuaScripting})
#include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
#target_link_libraries(LuaScripting PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
#target_compile_definitions(LuaScripting PRIVATE ${DESKTOP} -DDEMO_NAME=LuaScripting -D_LuaScripting ${CONTEXT_DEFINITION})

add_executable(ParallaxMapping ${SOURCE_ParallaxMapping})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(ParallaxMapping PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(ParallaxMapping PRIVATE ${DESKTOP} -DDEMO_NAME=ParallaxMapping -D_ParallaxMapping ${CONTEXT_DEFINITION})

add_executable(ParticlesExample ${SOURCE_ParticlesExample})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(ParticlesExample PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(ParticlesExample PRIVATE ${DESKTOP} -DDEMO_NAME=ParticlesExample -D_ParticlesExample ${CONTEXT_DEFINITION})

add_executable(PickingPainterMethod ${SOURCE_PickingPainterMethod})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(PickingPainterMethod PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(PickingPainterMethod PRIVATE ${DESKTOP} -DDEMO_NAME=PickingPainterMethod -D_PickingPainterMethod ${CONTEXT_DEFINITION})

add_executable(SkeletonAnimationExample ${SOURCE_SkeletonAnimationExample})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(SkeletonAnimationExample PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(SkeletonAnimationExample PRIVATE ${DESKTOP} -DDEMO_NAME=SkeletonAnimationExample -D_SkeletonAnimationExample ${CONTEXT_DEFINITION})

#add_executable(RacingGame ${SOURCE_RacingGame})
#include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
#target_link_libraries(RacingGame PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
#target_compile_definitions(RacingGame PRIVATE ${DESKTOP} -DDEMO_NAME=RacingGame -D_RacingGame ${CONTEXT_DEFINITION})

add_executable(SimplePhysics ${SOURCE_SimplePhysics})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(SimplePhysics PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(SimplePhysics PRIVATE ${DESKTOP} -DDEMO_NAME=SimplePhysics -D_SimplePhysics ${CONTEXT_DEFINITION})

add_executable(SSAOExample ${SOURCE_SSAOExample})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(SSAOExample PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(SSAOExample PRIVATE ${DESKTOP} -DDEMO_NAME=SSAOExample -D_SSAOExample ${CONTEXT_DEFINITION})

add_executable(TextRendering ${SOURCE_TextRendering})
include_directories(${HEADERS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB})
target_link_libraries(TextRendering PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)
target_compile_definitions(TextRendering PRIVATE ${DESKTOP} -DDEMO_NAME=TextRendering -D_TextRendering ${CONTEXT_DEFINITION})

file(COPY ${CMAKE_SOURCE_DIR}/resources/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
