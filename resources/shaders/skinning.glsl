// Pyros3D the renderer - skinning: a skinned mesh's vertices placed by up to four of its bones each (their ids and weights,
// a vertex stream of their own), the bones from this frame's palette (a storage buffer: each skinned draw's bones from
// its base). BONES_BINDING: where the palette is in the pass's group.
#ifdef SKINNED
layout(set = 1, binding = BONES_BINDING, std430) readonly buffer Bones { mat4 bones[]; } uBones;
layout(location = 8) in vec4 aBonesID;
layout(location = 9) in vec4 aBonesWeight;
mat4 SkinMatrix(float base)
{
	int b = int(base);
	return uBones.bones[b + int(aBonesID.x)] * aBonesWeight.x + uBones.bones[b + int(aBonesID.y)] * aBonesWeight.y
		+ uBones.bones[b + int(aBonesID.z)] * aBonesWeight.z + uBones.bones[b + int(aBonesID.w)] * aBonesWeight.w;
}
#else
mat4 SkinMatrix(float base) { return mat4(1.0); }
#endif
