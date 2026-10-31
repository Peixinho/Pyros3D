#version 450
#include "common.glsl"
// The depth prepass: the camera's depth (for GTAO, the reflections, the temporal resolve) and how far each surface moved
// on screen since last frame (its object's matrix then and now - DrawData - through last frame's and this frame's
// unjittered views): what the temporal anti-aliasing follows
// (skin.x: its first bone, SKINNED; y: seconds now, z: last frame's - for a plant's sway, wind: wind.glsl)
// (ALPHA - an alpha-cut surface's: uvTransform its texcoord's scale and offset, alpha.x its cutoff - depthalpha.frag)
layout(set = 0, binding = 0, std140) uniform DrawData { mat4 model; mat4 previousModel; vec4 skin; vec4 wind; vec4 uvTransform; vec4 alpha; vec4 shape; } uDraw;
// (shape: an instanced set's grown and faded instances, as the surface draws them - instancing.glsl)
layout(set = 1, binding = 1, std140) uniform TaaData
{
	mat4 inverseViewProjection;
	mat4 previousViewProjection;     // (last frame's, without its jitter)
	mat4 viewProjection;             // (this frame's, without its jitter)
	vec4 params;
} uTaa;
#include "instancing.glsl"
#define BONES_BINDING 2
#include "skinning.glsl"
#include "wind.glsl"
layout(location = 0) in vec3 aPosition;
layout(location = 0) out vec4 vCurrent;
layout(location = 1) out vec4 vPrevious;
#ifdef ALPHA
layout(location = 2) in vec2 aTexcoord;
layout(location = 2) out vec2 vTexcoord;
#endif
void main()
{
	// (posed with this frame's bones at both ends: the move is the object's and the camera's, not the limbs')
	// (and swayed by the wind now and as it was a frame ago: the leaf's own motion, which the temporal resolve follows)
	mat4 instance = ShapedInstance(InstanceMatrix(), uDraw.shape, uDraw.model, uFrame.cameraPosition.xyz);
	vec3 origin = (uDraw.model * instance)[3].xyz;
	vec3 posed = (SkinMatrix(uDraw.skin.x) * vec4(aPosition, 1.0)).xyz;
	vec4 local = instance * vec4(WindSway(posed, origin, uDraw.wind, uDraw.skin.y), 1.0);
	vec4 localBefore = instance * vec4(WindSway(posed, origin, uDraw.wind, uDraw.skin.z), 1.0);
	vec4 world = uDraw.model * local;
	gl_Position = uFrame.viewProjection * world;
	vCurrent = uTaa.viewProjection * world;
	vPrevious = uTaa.previousViewProjection * (uDraw.previousModel * localBefore);
#ifdef ALPHA
	vTexcoord = aTexcoord * uDraw.uvTransform.xy + uDraw.uvTransform.zw;
#endif
}
