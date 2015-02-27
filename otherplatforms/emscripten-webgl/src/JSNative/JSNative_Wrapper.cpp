#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Capsule.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cone.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cylinder.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Torus.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/TorusKnot.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include "JSNative_Wrapper.h"
#include "glue.cpp"
void PyrosJSNative::CreateContext() {
	EmscriptenWebGLContextAttributes ctxAttrs;
    emscripten_webgl_init_context_attributes(&ctxAttrs);
    ctx = emscripten_webgl_create_context(nullptr, &ctxAttrs);
    emscripten_webgl_make_context_current(ctx);
}