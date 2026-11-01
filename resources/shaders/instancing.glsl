// Pyros3D the renderer - an instanced draw's own data: each instance's matrix (four columns) and tint, from a second
// vertex stream that steps once per instance; the object's matrix (DrawData) places the whole set
#ifdef INSTANCED
layout(location = 3) in vec4 iModel0;
layout(location = 4) in vec4 iModel1;
layout(location = 5) in vec4 iModel2;
layout(location = 6) in vec4 iModel3;
layout(location = 7) in vec4 iTint;
// (its first column's w - nothing in an affine matrix - is its rank among its set, 0..1: which go first when it thins)
mat4 InstanceMatrix() { return mat4(vec4(iModel0.xyz, 0.0), vec4(iModel1.xyz, 0.0), iModel2, iModel3); }
float InstanceRank() { return iModel0.w; }
// (its second column's w: its level of detail's crossfade, from the GPU culling - instance_cull.comp; 0 whole)
float InstanceLodCode() { return iModel1.w; }
vec4 InstanceTint() { return iTint; }
// An instance as its set says to draw it now (DrawData's shape): x (two halves) how much wider and taller - a thinned
// layer's plants grown to cover what the ones let go covered - and y how much of the set is kept (1 all; 0 shape unset);
// z, w (two halves each: from, to - metres) faded in and out with its own distance from the eye: a far forest's trees
// coming in where the near ones end and going at the horizon. A plant going - its rank past what is kept, or past its
// fade - dissolves on the moving dither (gInstanceFade, for the cut and the prepass: alphacut.frag, depthalpha.frag)
// instead of shrinking or vanishing; gone altogether, it is a point, no triangle drawn
const float kThinBand = 0.08;     // (how much of the set is dissolving at once as it thins)
float gInstanceFade = 1.0;
vec2 ShapeGrow(vec4 shape) { return shape.y > 0.0 ? unpackHalf2x16(floatBitsToUint(shape.x)) : vec2(1.0); }
mat4 ShapedInstance(mat4 instance, vec4 shape, mat4 model, vec3 eye)
{
	vec2 grow = ShapeGrow(shape);
	float fade = 1.0;
	if (shape.y > 0.0 && shape.y < 1.0) fade = clamp((shape.y - InstanceRank()) / kThinBand, 0.0, 1.0);
	vec2 fadeIn = unpackHalf2x16(floatBitsToUint(shape.z)), fadeOut = unpackHalf2x16(floatBitsToUint(shape.w));
	if (fadeIn.y > fadeIn.x || fadeOut.y > fadeOut.x)
	{
		float d = distance((model * instance)[3].xyz, eye);
		if (fadeIn.y > fadeIn.x) fade *= smoothstep(fadeIn.x, fadeIn.y, d);
		if (fadeOut.y > fadeOut.x) fade *= 1.0 - smoothstep(fadeOut.x, fadeOut.y, d);
	}
	gInstanceFade = fade;
	if (fade <= 0.001) grow = vec2(0.0);
	instance[0] *= grow.x; instance[1] *= grow.y; instance[2] *= grow.x;
	return instance;
}
// (grown only - a pass that has no eye to fade by: the shadows)
mat4 GrownInstance(mat4 instance, vec4 shape)
{
	vec2 grow = ShapeGrow(shape);
	instance[0] *= grow.x; instance[1] *= grow.y; instance[2] *= grow.x;
	return instance;
}
#else
mat4 InstanceMatrix() { return mat4(1.0); }
float InstanceRank() { return 0.0; }
float InstanceLodCode() { return 0.0; }
float gInstanceFade = 1.0;
vec4 InstanceTint() { return vec4(1.0); }
mat4 ShapedInstance(mat4 instance, vec4 shape, mat4 model, vec3 eye) { return instance; }
mat4 GrownInstance(mat4 instance, vec4 shape) { return instance; }
#endif
