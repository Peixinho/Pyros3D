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

# ImGui backend glue. imgui_impl_sdl2.cpp is needed either way;
# imgui_impl_opengl3.cpp is OpenGL-only and must be left out entirely under
# CONTEXT=SDL2Vulkan - BaseExample.cpp never calls it there (see its
# `#if !defined(_SDL2VULKAN)` guards around every ImGui_ImplOpenGL3_* call),
# so compiling it into every Vulkan example was dead weight that also
# dragged in real OpenGL dependencies for a build that never touches OpenGL.
set(IMGUI_BACKEND_SOURCE ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp)
if (NOT CONTEXT STREQUAL "SDL2Vulkan")
	list(APPEND IMGUI_BACKEND_SOURCE ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
endif()

# freebsd
link_directories("/usr/local/lib")

# Headers shared by both example targets.
set(HEADERS
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}
	"/usr/local/include"
)
include_directories(${HEADERS} ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${CONTEXT_LIB} ${IMGUI_INCLUDE_DIRS})

# Minimal traditional C++ API demo (ForwardRenderer + cube + light).
set(SOURCE_CppApiDemo
	${CMAKE_CURRENT_SOURCE_DIR}/CppApiDemo/CppApiDemo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BaseExample/BaseExample.cpp
	${IMGUI_BACKEND_SOURCE}
	${WINDOW_MANAGER}
	${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
)
add_executable(CppApiDemo ${SOURCE_CppApiDemo})
target_link_libraries(CppApiDemo PyrosEngine ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
target_compile_definitions(CppApiDemo PRIVATE ${DESKTOP} -DDEMO_NAME=CppApiDemo -D_CppApiDemo ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})

# Unified demo browser - scene JSON + Lua. Requires real Lua bindings.
if (HAVE_LUA_BINDINGS)
	set(SOURCE_DemoLauncher
		${CMAKE_CURRENT_SOURCE_DIR}/DemoLauncher/DemoLauncher.cpp
		${IMGUI_BACKEND_SOURCE}
		${WINDOW_MANAGER}
		${CMAKE_CURRENT_SOURCE_DIR}/MainProgram.cpp
	)
	add_executable(DemoLauncher ${SOURCE_DemoLauncher})
	target_link_libraries(DemoLauncher PyrosEngine ${SDL2_LIBRARIES} ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} ${LUA_LIBRARIES})
	target_compile_definitions(DemoLauncher PRIVATE ${DESKTOP} -DDEMO_NAME=DemoLauncher -D_DemoLauncher ${CONTEXT_DEFINITION} -DEXAMPLES_PATH=${EXAMPLES_PATH})
endif()

# symlink, not file(COPY): a copy goes stale the moment resources/shaders/*.glsl
# changes, since this line only re-runs on a cmake reconfigure, not on every
# build - real source of a very confusing "edited the shader but nothing
# changed" session. A symlink always reflects the live source tree.
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/shaders)
	file(CREATE_LINK ${CMAKE_SOURCE_DIR}/resources/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders SYMBOLIC)
endif()
