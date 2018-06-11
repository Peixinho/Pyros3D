cmake_minimum_required(VERSION 3.1)

project(AssimpImporter)

set(CMAKE_CXX_STANDARD 11)


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


# Find OpenGL
find_package(OpenGL REQUIRED)
if (NOT OPENGL_FOUND)
	message(ERROR " OpenGL not found!")
endif(NOT OPENGL_FOUND)
# Find Assimp


find_package(ASSIMP REQUIRED)
if (NOT ASSIMP_FOUND)
	message(ERROR " Assimp not found!")
endif(NOT ASSIMP_FOUND)


# Set OpenGL Version
SET(OPENGL_VERSION "GL45" CACHE STRING "OpenGL Version")
if (OPENGL_VERSION STREQUAL "GL45")
	add_definitions(-DGL45)
	SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gl45/glad.c)
	else()
		if (OPENGL_VERSION STREQUAL "GLES2")
			add_definitions(-DGLES2)
			SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gles2/glad.c)
		else()
		if (OPENGL_VERSION STREQUAL "GLES3")
			add_definitions(-DGLES3)
			SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gles3/glad.c)
		endif()
	endif()
endif()


# Lib Type
option(STATIC_LIB "Build Static Lib" OFF)
if (STATIC_LIB)
	SET(LIB_TYPE STATIC)
else()
	SET(LIB_TYPE SHARED)
endif()


# Engine Sources
set(SOURCE
	${CMAKE_CURRENT_SOURCE_DIR}/src/AssimpAnimationImporter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/AssimpModelImporter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/MainProgram.cpp
)


# Headers
set(HEADERS
	${HEADERS}
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Engine Library
add_executable(AssimpImporter ${SOURCE})

target_link_libraries(AssimpImporter PyrosEngine ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} pthread dl)
