cmake_minimum_required(VERSION 3.11)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Pyros3D)

set(CMAKE_CXX_STANDARD 17)

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
	add_definitions(-DDESKTOP)
endif(IS_DESKTOP)


# LUA Support
option(HAVE_LUA_BINDINGS "Use LUA Bindings" ON)
if (HAVE_LUA_BINDINGS)
	SET(LUA_BINDINGS -DLUA_BINDINGS)
	add_definitions(-DLUA_BINDINGS)

	# sol.hpp (bundled) only supports Lua 5.1-5.4. Homebrew's unversioned
	# "lua" formula has moved to 5.5+, so prefer the versioned lua@5.4 keg
	# when present instead of letting FindLua51 pick up whatever "lua" resolves to.
	if (APPLE)
		execute_process(COMMAND brew --prefix lua@5.4
			OUTPUT_VARIABLE HOMEBREW_LUA54_PREFIX
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		if (HOMEBREW_LUA54_PREFIX AND EXISTS "${HOMEBREW_LUA54_PREFIX}/include/lua/lua.h")
			set(LUA_INCLUDE_DIR "${HOMEBREW_LUA54_PREFIX}/include/lua" CACHE PATH "Lua include directory")
			set(LUA_LIBRARY "${HOMEBREW_LUA54_PREFIX}/lib/liblua.dylib" CACHE FILEPATH "Lua library")
		endif()
	endif()

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
        SET(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/Pyros3D/Ext/gl45)
	# Find OpenGL
	find_package(OpenGL REQUIRED)
	if (NOT OPENGL_FOUND)
		message(ERROR " OpenGL not found!")
	endif(NOT OPENGL_FOUND)
	SET(OPENGL_LIBS ${OPENGL_LIBRARIES})
elseif (OPENGL_VERSION STREQUAL "GL42")
	add_definitions(-DGL42)
	SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gl42/glad.c)
        SET(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/Pyros3D/Ext/gl42)
	# Find OpenGL
	find_package(OpenGL REQUIRED)
	if (NOT OPENGL_FOUND)
		message(ERROR " OpenGL not found!")
	endif(NOT OPENGL_FOUND)
	SET(OPENGL_LIBS ${OPENGL_LIBRARIES})
elseif (OPENGL_VERSION STREQUAL "GL41")
	add_definitions(-DGL41)
	SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gl41/glad.c)
        SET(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/Pyros3D/Ext/gl41)
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
                        SET(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/Pyros3D/Ext/gles3)
		endif(IS_DESKTOP)
	endif()
endif()


# SPIR-V tooling (Vulkan roadmap Phase 2 - GLSL->SPIR-V cross-compilation +
# reflection; optional, auto-detected, and unused by anything yet since
# there's no VulkanRenderDevice to consume it - see VULKAN_ROADMAP.md).
# Neither shaderc nor spirv-cross ship CMake config files via Homebrew, so
# this follows the same "brew --prefix + find_path/find_library" pattern
# already used above for lua@5.4.
option(BUILD_SPIRV_TOOLING "Build the GLSL->SPIR-V compiler (needs shaderc + spirv-cross)" ON)
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

	if (SHADERC_INCLUDE_DIR AND SHADERC_LIBRARY AND SPIRV_CROSS_INCLUDE_DIR AND SPIRV_CROSS_CORE_LIBRARY AND SPIRV_CROSS_GLSL_LIBRARY AND SPIRV_CROSS_CPP_LIBRARY)
		set(SPIRV_TOOLING_FOUND ON)
		add_definitions(-DSPIRV_TOOLING)
		set(SPIRV_TOOLING_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/SPIRV/ShaderCompiler.cpp)
		set(SPIRV_TOOLING_INCLUDE_DIRS ${SHADERC_INCLUDE_DIR} ${SPIRV_CROSS_INCLUDE_DIR})
		# spirv-cross-cpp must come before spirv-cross-core (it depends on
		# core; static-lib link order matters on most linkers).
		set(SPIRV_TOOLING_LIBS ${SHADERC_LIBRARY} ${SPIRV_CROSS_CPP_LIBRARY} ${SPIRV_CROSS_GLSL_LIBRARY} ${SPIRV_CROSS_CORE_LIBRARY})
		message(STATUS "SPIR-V tooling found: shaderc=${SHADERC_LIBRARY}, spirv-cross=${SPIRV_CROSS_CORE_LIBRARY}")
	else()
		set(SPIRV_TOOLING_FOUND OFF)
		message(WARNING "shaderc/spirv-cross not found - the GLSL->SPIR-V compiler will not be built. Install via 'brew install shaderc spirv-cross'.")
	endif()
endif()


# Vulkan backend (Vulkan roadmap Phase 5 - VulkanRenderDevice; optional,
# off by default since it's still a stub with nothing consuming it yet -
# see VULKAN_ROADMAP.md). Needs two things: the Vulkan SDK pieces
# (vulkan-headers/vulkan-loader - found via CMake's built-in
# FindVulkan.cmake, available since CMake 3.7) and the real lightweight
# Vulkan loader (zeux/volk - note the package literally named "volk" on
# Homebrew is an unrelated DSP/signal-processing library, not this).
# Both volk and VMA (Vulkan Memory Allocator) are pulled via FetchContent
# rather than a package-manager lookup - neither has a Homebrew formula
# under this name, and this way the same CMake logic works unmodified on
# any platform's package manager (or lack thereof), including Windows.
option(BUILD_VULKAN_BACKEND "Build the Vulkan render device (needs the Vulkan SDK)" OFF)
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
		add_definitions(-DVULKAN_BACKEND)
		set(VULKAN_BACKEND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Device/VulkanRenderDevice.cpp)
		set(VULKAN_BACKEND_LIBS Vulkan::Vulkan volk::volk GPUOpen::VulkanMemoryAllocator)
		message(STATUS "Vulkan backend found: Vulkan=${Vulkan_LIBRARY}, volk+VMA fetched via FetchContent")
	else()
		set(VULKAN_BACKEND_FOUND OFF)
		message(WARNING "Vulkan SDK not found - the Vulkan render device will not be built. Install the LunarG Vulkan SDK (or 'brew install vulkan-headers vulkan-loader' on macOS).")
	endif()
endif()


# Lib Type
option(STATIC_LIB "Build Static Lib (only unix)" OFF)
if (STATIC_LIB)
	SET(LIB_TYPE STATIC)
else()
	SET(LIB_TYPE SHARED)
endif()


# Engine Sources
set(SOURCE
	${SOURCE}
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/Thread/Thread.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/ModelLoaders/MultiModelLoader/ModelLoader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/ModelLoaders/IModelLoader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/Mouse3D/Mouse3D.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/Mouse3D/PainterPick.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/Bindings/PyrosBindings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/Geometry/Geometry.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/FPS/FPS.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Utils/DeltaTime/DeltaTime.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Components/Lights/ILightComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Components/Lights/PointLight/PointLight.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Components/Rendering/RenderingComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Components/Particles/ParticleSystem.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Culling/Culling.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Culling/FrustumCulling/FrustumCulling.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/PostEffectsManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/IEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/BlurSSAOEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/VignetteEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/BlurXEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/BlurYEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/BloomEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/RTTDebug.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/ScreenSpaceReflectionEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/MotionBlurEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/DisplayTextureEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/SpecialRenderers/VelocityRenderer/VelocityRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/IRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Device/GLRenderDevice.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Device/IRenderDevice.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/GameObjects/GameObject.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Octree/Octree.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Logs/Log.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Buffers/FrameBuffer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Buffers/GeometryBuffer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/InputManager/InputManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/File/File.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Math/Matrix.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Math/Math.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Math/Vec4.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Math/Quaternion.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Math/Vec2.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Math/Vec3.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Math/Random.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Context/Context.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Core/Projection/Projection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Materials/Shaders/Shaders.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Materials/IMaterial.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/SceneGraph/SceneGraph.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/StringIDs/CRC32.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/StringIDs/StringID.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/Capsule/PhysicsCapsule.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/Cone/PhysicsCone.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/Sphere/PhysicsSphere.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/Cylinder/PhysicsCylinder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/Box/PhysicsBox.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/ConvexTriangleMesh/PhysicsConvexTriangleMesh.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/IPhysicsComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/MultipleSphere/PhysicsMultipleSphere.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/Vehicle/PhysicsVehicle.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/StaticPlane/PhysicsStaticPlane.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Components/ConvexHull/PhysicsConvexHull.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/Physics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/PhysicsEngines/BulletPhysics/DebugDraw/PhysicsDebugDraw.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/PhysicsEngines/BulletPhysics/BulletPhysics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Physics/PhysicsEngines/IPhysics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Resources/Resources.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/AnimationManager/SkeletonAnimation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/AnimationManager/TextureAnimation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Font/Font.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Models/Model.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Decals/Decals.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Renderables.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Text/Text.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Primitives/Primitive.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Primitives/Shapes/Capsule.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Primitives/Shapes/Cone.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Primitives/Shapes/TorusKnot.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Primitives/Shapes/Torus.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Primitives/Shapes/Cylinder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Renderable/Terrains/Terrain.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Assets/Texture/Texture.cpp
	${GL_INCLUDE}
	${SPIRV_TOOLING_SOURCE}
	${VULKAN_BACKEND_SOURCE}
)


# Headers
set(HEADERS
	${HEADERS}
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

include_directories(${HEADERS} ${GLAD_INCLUDE_DIR} ${LUA_INCLUDE_DIR} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS} ${SPIRV_TOOLING_INCLUDE_DIRS})

# Engine Library
add_library(PyrosEngine ${LIB_TYPE} ${SOURCE} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS})
target_link_libraries(PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${LUA_LIBRARIES} ${CONTEXT_LIB} ${SPIRV_TOOLING_LIBS} ${VULKAN_BACKEND_LIBS})
# Neither pthread nor dl is actually used anywhere in the engine (Thread.cpp
# is now std::thread-based - see Thread.cpp/.h - and nothing calls dlopen/
# dlsym) - both were dead link flags, and both fail to resolve on Windows.

# PYROS3D_API (Export.h) expands to __declspec(dllexport/dllimport) under
# _WIN32, gated on _EXPORT/_IMPORT - neither was ever actually defined
# anywhere in this build, so every dllexport/dllimport annotation in the
# codebase has always silently expanded to nothing on Windows. _EXPORT
# only makes sense for the actual DLL build (STATIC_LIB has no import/
# export boundary at all). Downstream consumers (examples/CMakeLists.txt)
# define _IMPORT the same way when linking against a shared PyrosEngine.
if (WIN32 AND LIB_TYPE STREQUAL "SHARED")
	target_compile_definitions(PyrosEngine PRIVATE _EXPORT)
endif()

# Model Converter
option(BUILD_CONVERTER "Build Model Converter" OFF)
if (BUILD_CONVERTER)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tools/AssimpImporter)
endif()


# Demos
option(BUILD_DEMOS "Build Demos" OFF)
if (BUILD_DEMOS)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
endif()
