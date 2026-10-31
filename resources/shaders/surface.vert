#version 450
#include "common.glsl"
#include "surface_draw.glsl"
#include "instancing.glsl"
#define BONES_BINDING 17
#include "skinning.glsl"
#include "wind.glsl"

#ifdef CUSTOM_VERTEX
// A material's own vertex stage (its surface source's `void Vertex(inout VertexData v)`): the world position, normal
// and texcoord, before they are projected. What its text may read:
#define P3D_STAGE_VERTEX 1
struct VertexData { vec3 position, normal; vec2 uv; };
#define uTime (uFrame.sky.z)
#define uCameraPosition (uFrame.cameraPosition.xyz)
#define uModelMatrix (uDraw.model)
#define uGlobals (uFrame.globals)
#define texture_2D texture
#define P3D_PI PI
//P3D_SURFACE_DECLARATIONS
//P3D_SURFACE
#endif

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexcoord;
#ifdef VERTEX_COLOR
layout(location = 10) in vec4 aColor;     // (a generic material's VertexColor: a sprite's tint and fade, per vertex - sRGB)
#endif

layout(location = 0) out vec3 vWorldPosition;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vTexcoord;
layout(location = 3) out vec4 vTint;

void main()
{
#ifdef INSTANCED
	mat4 model = uDraw.model * ShapedInstance(InstanceMatrix(), uDraw.shape, uDraw.model, uFrame.cameraPosition.xyz);
	vec4 world = model * vec4(WindSway(aPosition, model[3].xyz, uDraw.wind, uFrame.sky.z), 1.0);
	vNormal = transpose(inverse(mat3(model))) * aNormal;
#elif defined(SKINNED)
	// (posed by its bones, then placed by its object)
	mat4 skin = SkinMatrix(uDraw.skin.x);
	vec4 world = uDraw.model * (skin * vec4(aPosition, 1.0));
	vec3 n = mat3(skin) * aNormal;
	vNormal = vec3(dot(uDraw.normalRows[0].xyz, n), dot(uDraw.normalRows[1].xyz, n), dot(uDraw.normalRows[2].xyz, n));
#else
	vec4 world = uDraw.model * vec4(WindSway(aPosition, uDraw.model[3].xyz, uDraw.wind, uFrame.sky.z), 1.0);
	vNormal = vec3(dot(uDraw.normalRows[0].xyz, aNormal), dot(uDraw.normalRows[1].xyz, aNormal), dot(uDraw.normalRows[2].xyz, aNormal));
#endif
	// (a blade of grass is a card on end: lit by its own face a field of them is half dark whichever way the sun is;
	// leaning to the sky, it takes the light the ground under it takes)
	if (uDraw.wind.w > 0.0) vNormal = mix(normalize(vNormal), vec3(0.0, 1.0, 0.0), min(uDraw.wind.w, 1.0));
	vTint = InstanceTint();
#ifdef VERTEX_COLOR
	vTint *= vec4(SrgbDecode(aColor.rgb), aColor.a);
#endif
	vTexcoord = aTexcoord * uDraw.uvTransform.xy + uDraw.uvTransform.zw;
#ifdef CUSTOM_VERTEX
	VertexData vd;
	vd.position = world.xyz; vd.normal = vNormal; vd.uv = aTexcoord * uDraw.uvTransform.xy + uDraw.uvTransform.zw;
	Vertex(vd);
	world = vec4(vd.position, 1.0); vNormal = vd.normal;
	vTexcoord = vd.uv;
#endif
	vWorldPosition = world.xyz;
	gl_Position = uFrame.viewProjection * world;
}
