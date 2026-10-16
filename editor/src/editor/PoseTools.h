//=============================================================================
// Name        : PoseTools.h
// Description : Pose operations shared by the 3D Animation Editor and the
//               Character 2D editor: left/right counterparts and mirroring.
//
//               Mirroring works on each bone's change FROM ITS BIND POSE in
//               model space, not on its local matrix. A left and a right arm
//               rarely have mirrored local axes (one rig's left forearm
//               twists about +X, the right one about -X), so flipping a local
//               rotation's sign is right on one rig and wrong on the next.
//               The model-space delta does not care: mirror it across the
//               plane, put it on the counterpart's own bind, and the axes
//               come out however that bone was modelled.
//=============================================================================

#ifndef POSETOOLS_H
#define POSETOOLS_H

#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <map>
#include <string>
#include <vector>

namespace PoseTools {

// The other side's name for `name` ("LeftArm" -> "RightArm", "hand.L" ->
// "hand.R", "l_foot" -> "r_foot", "mixamorig:LeftLeg" -> "mixamorig:RightLeg"),
// or empty when the name has no side in it.
std::string CounterpartName(const std::string& name);

// The counterpart's bone id in `inst`, the bone itself for a centre bone
// (spine, head), -1 for a bad id.
int CounterpartBone(p3d::SkeletonAnimationInstance* inst, int boneId);

// `locals` (one per bone, the instance's own layout) mirrored across the
// model-space plane normal to `axis` (0 = X, the usual left/right; 1 = Y;
// 2 = Z). Every bone takes its counterpart's mirrored pose; centre bones
// mirror their own.
std::vector<p3d::Math::Matrix> MirrorPose(p3d::SkeletonAnimationInstance* inst,
	const std::vector<p3d::Math::Matrix>& locals, int axis = 0);

// ---- retargeting -------------------------------------------------------------
//
// A clip authored for one skeleton, played on another: each target bone takes
// its source bone's change from bind IN MODEL SPACE (the mirror's trick, so
// differently-rolled local axes do not matter) and keeps its own bone lengths.
// The root's travel is scaled by the ratio of the two rigs' root heights, so a
// short character's walk does not slide.

// A bone name reduced to what two exporters agree on: no namespace
// ("mixamorig:"), no "Bip01"/"Armature" prefix, no separators, lower case.
std::string NormalizeBoneName(const std::string& name);

// target bone name -> source bone name, for every target bone whose
// normalised name matches a source bone's.
std::map<std::string, std::string> AutoBoneMap(p3d::SkeletonAnimationInstance* source,
	p3d::SkeletonAnimationInstance* target);

// `clip` (keyed against `source`) re-keyed for `target`, sampled `fps` times a
// second. One rotation key per mapped bone per sample; position keys only on
// the mapped root. Leaves both instances posed at the last sample.
p3d::Animation RetargetClip(p3d::SkeletonAnimationInstance* source, p3d::SkeletonAnimationInstance* target,
	const p3d::Animation& clip, const std::map<std::string, std::string>& targetToSource, float fps);

} // namespace PoseTools

#endif
