LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CXXFLAGS    := -D_ANDROID -D_SDL2 -DLODEPNG
LOCAL_MODULE       := PyrosEngine
LOCAL_C_INCLUDES   :=   $(LOCAL_PATH)/../../../include \
                        $(LOCAL_PATH)/../../../src/Pyros3D \
                        $(LOCAL_PATH)/../../../src/Pyros3D/AnimationManager \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Assets/Renderable \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Assets/Renderable/Models \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Assets/Renderable/Primitives \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Assets/Renderable/Primitives/Shapes \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Renderable/Text \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Assets/Sounds \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Components \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Core \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Core/Buffers \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Core/Context \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Core/InputManager \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Core/Logs \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Core/Math \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Core/Projection \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Ext/Signals \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Ext/StringIDs \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Ext/lodepng \
                        $(LOCAL_PATH)/../../../src/Pyros3D/GameObjects \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Materials \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Materials/CustomShaderMaterials \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Materials/GenericShaderMaterials \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Materials/Shaders \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Components \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Components/Lights \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Components/Lights/DirectionalLight \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Components/Lights/PointLight \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Components/Lights/SpotLight \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Components/Rendering \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Culling \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Culling/FrustumCulling \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Renderer \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Renderer/ForwardRenderer \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Renderer/SpecialRenderers \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer \
                        $(LOCAL_PATH)/../../../src/Pyros3D/SceneGraph \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Utils \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Utils/Binary \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Utils/Colors \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Utils/DeltaTime \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Utils/FPS \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Utils/Geometry \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Utils/IniParser \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Utils/ModelLoaders \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Utils/Mouse3D \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/Vehicle \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/Cylinder \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/TriangleMesh \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/Sphere \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/MultipleSPhere \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/StaticPlane \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/Box \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/Cone \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/Capsule \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/ConvexHull \
                        $(LOCAL_PATH)/../../../src/Pyros3D/Physics/Components/ConvexTriangleMesh \
                        $(LOCAL_PATH)/../src \
                        $(LOCAL_PATH)/../../include



LOCAL_SRC_FILES    := ../../../src/Pyros3D/Materials/IMaterial.cpp ../../../src/Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.cpp ../../../src/Pyros3D/Materials/GenericShaderMaterials/ShaderLib.cpp ../../../src/Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.cpp ../../../src/Pyros3D/Materials/Shaders/Shaders.cpp ../../../src/Pyros3D/Utils/FPS/FPS.cpp ../../../src/Pyros3D/Utils/ModelLoaders/IModelLoader.cpp ../../../src/Pyros3D/Utils/ModelLoaders/MultiModelLoader/ModelLoader.cpp ../../../src/Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.cpp ../src/File.cpp ../../../src/Pyros3D/Utils/DeltaTime/DeltaTime.cpp ../../../src/Pyros3D/Utils/Geometry/Geometry.cpp ../../../src/Pyros3D/Utils/Mouse3D/Mouse3D.cpp ../../../src/Pyros3D/Utils/Mouse3D/PainterPick.cpp ../../../src/Pyros3D/Core/InputManager/InputManager.cpp ../../../src/Pyros3D/Core/Context/Context.cpp ../../../src/Pyros3D/Core/Math/Vec2.cpp ../../../src/Pyros3D/Core/Math/Matrix.cpp ../../../src/Pyros3D/Core/Math/Vec4.cpp ../../../src/Pyros3D/Core/Math/Vec3.cpp ../../../src/Pyros3D/Core/Math/Quaternion.cpp ../../../src/Pyros3D/Core/Projection/Projection.cpp ../../../src/Pyros3D/Core/Buffers/FrameBuffer.cpp ../../../src/Pyros3D/Core/Buffers/GeometryBuffer.cpp ../../../src/Pyros3D/Core/Logs/Log.cpp ../../../src/Pyros3D/Assets/Renderable/Renderables.cpp ../../../src/Pyros3D/Assets/Renderable/Models/Model.cpp ../../../src/Pyros3D/Assets/Renderable/Text/Text.cpp ../../../src/Pyros3D/Assets/Font/Font.cpp ../../../src/Pyros3D/Assets/Texture/Texture.cpp ../../../src/Pyros3D/Assets/Sounds/ISound.cpp ../../../src/Pyros3D/Assets/Sounds/Music.cpp ../../../src/Pyros3D/Assets/Sounds/Sound.cpp ../../../src/Pyros3D/Rendering/Culling/Culling.cpp ../../../src/Pyros3D/Rendering/Culling/FrustumCulling/FrustumCulling.cpp ../../../src/Pyros3D/Rendering/Components/Rendering/RenderingComponent.cpp ../../../src/Pyros3D/Rendering/Components/Lights/ILightComponent.cpp ../../../src/Pyros3D/Rendering/Renderer/IRenderer.cpp ../../../src/Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.cpp ../../../src/Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.cpp ../../../src/Pyros3D/SceneGraph/SceneGraph.cpp ../../../src/Pyros3D/GameObjects/GameObject.cpp ../../../src/Pyros3D/Ext/lodepng/lodepng.cpp ../../../src/Pyros3D/Ext/StringIDs/CRC32.cpp ../../../src/Pyros3D/Ext/StringIDs/StringID.cpp ../../../src/Pyros3D/AnimationManager/AnimationManager.cpp ../../../src/Pyros3D/AnimationManager/TextureAnimation.cpp ../../../src/Pyros3D/Physics/Physics.cpp ../../../src/Pyros3D/Physics/PhysicsEngines/IPhysics.cpp ../../../src/Pyros3D/Physics/PhysicsEngines/BulletPhysics/BulletPhysics.cpp ../../../src/Pyros3D/Physics/PhysicsEngines/BulletPhysics/DebugDraw/PhysicsDebugDraw.cpp ../../../src/Pyros3D/Physics/Components/IPhysicsComponent.cpp ../../../src/Pyros3D/Physics/Components/Vehicle/PhysicsVehicle.cpp ../../../src/Pyros3D/Physics/Components/Cylinder/PhysicsCylinder.cpp ../../../src/Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.cpp ../../../src/Pyros3D/Physics/Components/Sphere/PhysicsSphere.cpp ../../../src/Pyros3D/Physics/Components/MultipleSphere/PhysicsMultipleSphere.cpp ../../../src/Pyros3D/Physics/Components/StaticPlane/PhysicsStaticPlane.cpp ../../../src/Pyros3D/Physics/Components/Box/PhysicsBox.cpp ../../../src/Pyros3D/Physics/Components/Cone/PhysicsCone.cpp ../../../src/Pyros3D/Physics/Components/Capsule/PhysicsCapsule.cpp ../../../src/Pyros3D/Physics/Components/ConvexHull/PhysicsConvexHull.cpp ../../../src/Pyros3D/Physics/Components/ConvexTriangleMesh/PhysicsConvexTriangleMesh.cpp

LOCAL_CPP_FEATURES := rtti exceptions
LOCAL_CFLAGS       := -O3 -fPIC
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE           := PyrosEngineShared
LOCAL_STATIC_LIBRARIES := PyrosEngine
include $(BUILD_SHARED_LIBRARY)
