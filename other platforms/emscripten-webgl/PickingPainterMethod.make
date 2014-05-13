# Emscripten Make
ifndef config
  config=debug32
endif

ifndef verbose
  SILENT = @
endif

CC = emcc
CXX = em++
AR = ar

ifndef RESCOMP
  ifdef WINDRES
    RESCOMP = $(WINDRES)
  else
    RESCOMP = windres
  endif
endif

ifeq ($(config),debug32)
  OBJDIR     = obj/x32/Debug
  TARGETDIR  = bin
  TARGET     = $(TARGETDIR)/PickingPainterMethodDebug.html
  DEFINES   += -DUNICODE -DGLEW_STATIC -D_SDL -DLOG_TO_CONSOLE -D_DEBUG -DEMSCRIPTEN -D_PickingPainterMethod -DDEMO_NAME=PickingPainterMethod
  INCLUDES  += -I../../include -I../../src
  ALL_CPPFLAGS  += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS    += $(CFLAGS) $(ALL_CPPFLAGS) $(ARCH) -g -m32 -fPIC -s FULL_ES2=1
  ALL_CXXFLAGS  += $(CXXFLAGS) $(ALL_CFLAGS) -L../../libs
  ALL_RESFLAGS  += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  ALL_LDFLAGS   += $(LDFLAGS) -shared -m32 -L../../libs -lfreeimage
  EMSCRIPTEN-PRELOAD = --preload-file ../../examples/PickingPainterMethod/assets@../../../../examples/PickingPainterMethod/assets
  LDDEPS    +=
  LIBS      += $(LDDEPS)
  LINKCMD    = $(CXX) -o $(TARGET) $(OBJECTS) $(RESOURCES) $(ARCH) $(ALL_LDFLAGS) $(LIBS) $(EMSCRIPTEN-PRELOAD)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
endif

ifeq ($(config),release32)
  OBJDIR     = obj/x32/Release
  TARGETDIR  = bin
  TARGET     = $(TARGETDIR)/PickingPainterMethodRelease.html
  DEFINES   += -DUNICODE -DGLEW_STATIC -D_SDL -DLOG_DISABLE -DEMSCRIPTEN -D_PickingPainterMethod -DDEMO_NAME=PickingPainterMethod
  INCLUDES  += -I../../include
  ALL_CPPFLAGS  += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS    += $(CFLAGS) $(ALL_CPPFLAGS) $(ARCH) -O2 -m32 -fPIC
  ALL_CXXFLAGS  += $(CXXFLAGS) $(ALL_CFLAGS) -L../../libs
  ALL_RESFLAGS  += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  ALL_LDFLAGS   += $(LDFLAGS) -s -shared -m32 -L/usr/lib32 -L../../libs -lfreeimage
  EMSCRIPTEN-PRELOAD = --preload-file ../../examples/PickingPainterMethod/assets@../../../../examples/PickingPainterMethod/assets
  LDDEPS    +=
  LIBS      += $(LDDEPS)
  LINKCMD    = $(CXX) -o $(TARGET) $(OBJECTS) $(RESOURCES) $(ARCH) $(ALL_LDFLAGS) $(LIBS) $(EMSCRIPTEN-PRELOAD)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
endif

OBJECTS := \
	$(OBJDIR)/IMaterial.o \
	$(OBJDIR)/CustomShaderMaterial.o \
	$(OBJDIR)/ShaderLib.o \
	$(OBJDIR)/GenericShaderMaterial.o \
	$(OBJDIR)/Shaders.o \
	$(OBJDIR)/FPS.o \
	$(OBJDIR)/Context.o \
	$(OBJDIR)/SDLContext.o \
	$(OBJDIR)/IModelLoader.o \
	$(OBJDIR)/ModelLoader.o \
	$(OBJDIR)/AnimationLoader.o \
	$(OBJDIR)/Thread.o \
	$(OBJDIR)/DeltaTime.o \
	$(OBJDIR)/Geometry.o \
	$(OBJDIR)/Mouse3D.o \
	$(OBJDIR)/PainterPick.o \
	$(OBJDIR)/InputManager.o \
	$(OBJDIR)/Vec2.o \
	$(OBJDIR)/Matrix.o \
	$(OBJDIR)/Vec4.o \
	$(OBJDIR)/Vec3.o \
	$(OBJDIR)/Quaternion.o \
	$(OBJDIR)/Projection.o \
	$(OBJDIR)/FrameBuffer.o \
	$(OBJDIR)/GeometryBuffer.o \
	$(OBJDIR)/Log.o \
	$(OBJDIR)/AssetManager.o \
	$(OBJDIR)/Renderables.o \
	$(OBJDIR)/Model.o \
	$(OBJDIR)/Texture.o \
	$(OBJDIR)/Culling.o \
	$(OBJDIR)/FrustumCulling.o \
	$(OBJDIR)/RenderingComponent.o \
	$(OBJDIR)/ILightComponent.o \
	$(OBJDIR)/IRenderer.o \
	$(OBJDIR)/ForwardRenderer.o \
	$(OBJDIR)/CubemapRenderer.o \
	$(OBJDIR)/SceneGraph.o \
	$(OBJDIR)/AnimationManager.o \
	$(OBJDIR)/GameObject.o \
	$(OBJDIR)/CRC32.o \
	$(OBJDIR)/StringID.o \
	$(OBJDIR)/PickingPainterMethod.o \
	$(OBJDIR)/MainProgram.o

RESOURCES := \

SHELLTYPE := msdos
ifeq (,$(ComSpec)$(COMSPEC))
  SHELLTYPE := posix
endif
ifeq (/bin,$(findstring /bin,$(SHELL)))
  SHELLTYPE := posix
endif

.PHONY: clean prebuild prelink

all: $(TARGETDIR) $(OBJDIR) prebuild prelink $(TARGET)
	@:

$(TARGET): $(GCH) $(OBJECTS) $(LDDEPS) $(RESOURCES)
	@echo Linking PyrosEngine
	$(SILENT) $(LINKCMD)
	$(POSTBUILDCMDS)

$(TARGETDIR):
	@echo Creating $(TARGETDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(TARGETDIR)
else
	$(SILENT) mkdir $(subst /,\\,$(TARGETDIR))
endif

$(OBJDIR):
	@echo Creating $(OBJDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(OBJDIR)
else
	$(SILENT) mkdir $(subst /,\\,$(OBJDIR))
endif

clean:
	@echo Cleaning PyrosEngine
ifeq (posix,$(SHELLTYPE))
	$(SILENT) rm -f  $(TARGET)
	$(SILENT) rm -rf $(OBJDIR)
else
	$(SILENT) if exist $(subst /,\\,$(TARGET)) del $(subst /,\\,$(TARGET))
	$(SILENT) if exist $(subst /,\\,$(OBJDIR)) rmdir /s /q $(subst /,\\,$(OBJDIR))
endif

prebuild:
	$(PREBUILDCMDS)

prelink:
	$(PRELINKCMDS)

ifneq (,$(PCH))
$(GCH): $(PCH)
	@echo $(notdir $<)
	$(SILENT) $(CXX) -x c++-header $(ALL_CXXFLAGS) -MMD -MP $(DEFINES) $(INCLUDES) -o "$@" -MF "$(@:%.gch=%.d)" -c "$<"
endif

$(OBJDIR)/IMaterial.o: ../../src/Pyros3D/Materials/IMaterial.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/CustomShaderMaterial.o: ../../src/Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/ShaderLib.o: ../../src/Pyros3D/Materials/GenericShaderMaterials/ShaderLib.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/GenericShaderMaterial.o: ../../src/Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Shaders.o: ../../src/Pyros3D/Materials/Shaders/Shaders.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/FPS.o: ../../src/Pyros3D/Utils/FPS/FPS.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Context.o: ../../src/Pyros3D/Core/Context/Context.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/SDLContext.o: ../../examples/WindowManagers/SDL/SDLContext.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/IModelLoader.o: ../../src/Pyros3D/Utils/ModelLoaders/IModelLoader.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/ModelLoader.o: src/ModelLoader.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/AnimationLoader.o: src/AnimationLoader.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Thread.o: ../../src/Pyros3D/Utils/Thread/Thread.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/DeltaTime.o: ../../src/Pyros3D/Utils/DeltaTime/DeltaTime.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Geometry.o: ../../src/Pyros3D/Utils/Geometry/Geometry.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Mouse3D.o: ../../src/Pyros3D/Utils/Mouse3D/Mouse3D.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PainterPick.o: ../../src/Pyros3D/Utils/Mouse3D/PainterPick.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/InputManager.o: ../../src/Pyros3D/Core/InputManager/InputManager.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Vec2.o: ../../src/Pyros3D/Core/Math/Vec2.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Matrix.o: ../../src/Pyros3D/Core/Math/Matrix.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Vec4.o: ../../src/Pyros3D/Core/Math/Vec4.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Vec3.o: ../../src/Pyros3D/Core/Math/Vec3.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Quaternion.o: ../../src/Pyros3D/Core/Math/Quaternion.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Projection.o: ../../src/Pyros3D/Core/Projection/Projection.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/FrameBuffer.o: ../../src/Pyros3D/Core/Buffers/FrameBuffer.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/GeometryBuffer.o: ../../src/Pyros3D/Core/Buffers/GeometryBuffer.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Log.o: ../../src/Pyros3D/Core/Logs/Log.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/AssetManager.o: ../../src/Pyros3D/AssetManager/AssetManager.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Renderables.o: ../../src/Pyros3D/AssetManager/Assets/Renderable/Renderables.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Model.o: ../../src/Pyros3D/AssetManager/Assets/Renderable/Models/Model.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Text.o: ../../src/Pyros3D/AssetManager/Assets/Renderable/Text/Text.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Font.o: src/Font.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Texture.o: src/Texture.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Culling.o: ../../src/Pyros3D/Rendering/Culling/Culling.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/FrustumCulling.o: ../../src/Pyros3D/Rendering/Culling/FrustumCulling/FrustumCulling.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/RenderingComponent.o: ../../src/Pyros3D/Rendering/Components/Rendering/RenderingComponent.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/ILightComponent.o: ../../src/Pyros3D/Rendering/Components/Lights/ILightComponent.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/IRenderer.o: ../../src/Pyros3D/Rendering/Renderer/IRenderer.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/ForwardRenderer.o: ../../src/Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/CubemapRenderer.o: ../../src/Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/SceneGraph.o: ../../src/Pyros3D/SceneGraph/SceneGraph.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/AnimationManager.o: ../../src/Pyros3D/AnimationManager/AnimationManager.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/Physics.o: ../../src/Pyros3D/Physics/Physics.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/IPhysics.o: ../../src/Pyros3D/Physics/PhysicsEngines/IPhysics.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/BulletPhysics.o: ../../src/Pyros3D/Physics/PhysicsEngines/BulletPhysics/BulletPhysics.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsDebugDraw.o: ../../src/Pyros3D/Physics/PhysicsEngines/BulletPhysics/DebugDraw/PhysicsDebugDraw.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/IPhysicsComponent.o: ../../src/Pyros3D/Physics/Components/IPhysicsComponent.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsVehicle.o: ../../src/Pyros3D/Physics/Components/Vehicle/PhysicsVehicle.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsCylinder.o: ../../src/Pyros3D/Physics/Components/Cylinder/PhysicsCylinder.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsTriangleMesh.o: ../../src/Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsSphere.o: ../../src/Pyros3D/Physics/Components/Sphere/PhysicsSphere.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsMultipleSphere.o: ../../src/Pyros3D/Physics/Components/MultipleSphere/PhysicsMultipleSphere.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsStaticPlane.o: ../../src/Pyros3D/Physics/Components/StaticPlane/PhysicsStaticPlane.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsBox.o: ../../src/Pyros3D/Physics/Components/Box/PhysicsBox.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsCone.o: ../../src/Pyros3D/Physics/Components/Cone/PhysicsCone.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsCapsule.o: ../../src/Pyros3D/Physics/Components/Capsule/PhysicsCapsule.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsConvexHull.o: ../../src/Pyros3D/Physics/Components/ConvexHull/PhysicsConvexHull.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PhysicsConvexTriangleMesh.o: ../../src/Pyros3D/Physics/Components/ConvexTriangleMesh/PhysicsConvexTriangleMesh.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/GameObject.o: ../../src/Pyros3D/GameObjects/GameObject.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/CRC32.o: ../../src/Pyros3D/Ext/StringIDs/CRC32.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/StringID.o: ../../src/Pyros3D/Ext/StringIDs/StringID.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/PickingPainterMethod.o: ../../examples/PickingPainterMethod/PickingPainterMethod.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"
$(OBJDIR)/MainProgram.o: ../../examples/MainProgram.cpp
	@echo $(notdir $<)
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"



-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(OBJDIR)/$(notdir $(PCH)).d
endif
