cmake_minimum_required(VERSION 3.1)

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
        SET(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/Pyros3D/Ext/gl45)
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
                                SET(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/Pyros3D/Ext/gles2)
			endif(IS_DESKTOP)
		else()
		if (OPENGL_VERSION STREQUAL "GLES3")
			add_definitions(-DGLES3)
			if (IS_DESKTOP)
				SET(GL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Ext/gles3/glad.c)
                                SET(GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/Pyros3D/Ext/gles3)
			endif(IS_DESKTOP)
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
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/PostEffects/Effects/MotionBlurEffect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/SpecialRenderers/VelocityRenderer/VelocityRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Pyros3D/Rendering/Renderer/IRenderer.cpp
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
)


# Headers
set(HEADERS
	${HEADERS}
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

include_directories(${HEADERS} ${GLAD_INCLUDE_DIR} ${LUA_INCLUDE_DIR} ${OPENGL_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS})

# Engine Library
add_library(PyrosEngine ${LIB_TYPE} ${SOURCE} ${FREETYPE_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS})
target_link_libraries(PyrosEngine ${OPENGL_LIBS} ${BULLET_LIBRARIES} ${FREETYPE_LIBRARIES} ${ASSIMP_LIBRARIES} ${CONTEXT_LIB} pthread dl)

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
