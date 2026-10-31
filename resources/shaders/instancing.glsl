// Pyros3D the renderer - an instanced draw's own data: each instance's matrix (four columns) and tint, from a second
// vertex stream that steps once per instance; the object's matrix (DrawData) places the whole set
#ifdef INSTANCED
layout(location = 3) in vec4 iModel0;
layout(location = 4) in vec4 iModel1;
layout(location = 5) in vec4 iModel2;
layout(location = 6) in vec4 iModel3;
layout(location = 7) in vec4 iTint;
mat4 InstanceMatrix() { return mat4(iModel0, iModel1, iModel2, iModel3); }
vec4 InstanceTint() { return iTint; }
// An instance as its set says to draw it now (DrawData's shape): xy how much wider and taller - a thinned layer's plants
// grown to cover what the ones let go covered (0: as they are); z, w (two halves each: from, to - metres) faded in and
// out with its own distance from the eye: a far forest's trees coming in where the near ones end and going at the
// horizon. Faded, it shrinks to nothing - a point, no triangle drawn
mat4 ShapedInstance(mat4 instance, vec4 shape, mat4 model, vec3 eye)
{
	vec2 grow = shape.x > 0.0 ? shape.xy : vec2(1.0);
	vec2 fadeIn = unpackHalf2x16(floatBitsToUint(shape.z)), fadeOut = unpackHalf2x16(floatBitsToUint(shape.w));
	if (fadeIn.y > fadeIn.x || fadeOut.y > fadeOut.x)
	{
		float d = distance((model * instance)[3].xyz, eye);
		float s = 1.0;
		if (fadeIn.y > fadeIn.x) s *= smoothstep(fadeIn.x, fadeIn.y, d);
		if (fadeOut.y > fadeOut.x) s *= 1.0 - smoothstep(fadeOut.x, fadeOut.y, d);
		grow *= s;
	}
	instance[0] *= grow.x; instance[1] *= grow.y; instance[2] *= grow.x;
	return instance;
}
// (grown only - a pass that has no eye to fade by: the shadows)
mat4 GrownInstance(mat4 instance, vec2 shape)
{
	vec2 grow = shape.x > 0.0 ? shape : vec2(1.0);
	instance[0] *= grow.x; instance[1] *= grow.y; instance[2] *= grow.x;
	return instance;
}
#else
mat4 InstanceMatrix() { return mat4(1.0); }
vec4 InstanceTint() { return vec4(1.0); }
mat4 ShapedInstance(mat4 instance, vec4 shape, mat4 model, vec3 eye) { return instance; }
mat4 GrownInstance(mat4 instance, vec2 shape) { return instance; }
#endif
