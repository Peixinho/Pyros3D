#version 450
// A shadow caster into one cascade of the sun's map: the cascade's matrix by dynamic offset, the object's by DrawData.
// ALPHA: an alpha-cut material's - its texcoord passed on, for shadowalpha.frag to cut by its base colour's alpha

layout(set = 0, binding = 0, std140) uniform DrawData
{
	mat4 model;
	vec4 uvTransform;        // (ALPHA) xy scale, zw offset
	vec4 alpha;              // (ALPHA) x: the cutoff
	vec4 skin;               // (SKINNED) x: its first bone in the frame's palette; y: seconds (the wind's)
	vec4 wind;               // a plant's sway, as its surface sways - wind.glsl
	vec4 shape;              // an instanced set's plants grown as the surface draws them (instancing.glsl); its fades: the eye's
	                         // place - xyz of a second vec4 would be needed: a faded set casts no shadow (FarForest)
} uDraw;
layout(set = 1, binding = 0, std140) uniform Cascade { mat4 viewProjection; } uCascade;

#include "instancing.glsl"
#define BONES_BINDING 3
#include "skinning.glsl"
#include "wind.glsl"
layout(location = 0) in vec3 aPosition;
#ifdef ALPHA
layout(location = 2) in vec2 aTexcoord;
layout(location = 0) out vec2 vTexcoord;
#endif

void main()
{
#ifdef ALPHA
	vTexcoord = aTexcoord * uDraw.uvTransform.xy + uDraw.uvTransform.zw;
#endif
	mat4 instance = GrownInstance(InstanceMatrix(), uDraw.shape.xy);
	vec3 posed = (SkinMatrix(uDraw.skin.x) * vec4(aPosition, 1.0)).xyz;
	gl_Position = uCascade.viewProjection * (uDraw.model * instance * vec4(WindSway(posed, (uDraw.model * instance)[3].xyz, uDraw.wind, uDraw.skin.y), 1.0));
}
