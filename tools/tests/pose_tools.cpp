// The editor's pose tools (editor/src/editor/PoseTools): left/right
// counterparts by name, and mirroring a pose across the model's X = 0 plane.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/pose_tools.cpp editor/src/editor/PoseTools.cpp -o /tmp/pose_tools \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/pose_tools

#include "../../editor/src/editor/PoseTools.h"
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <cmath>
#include <cstdio>
#include <memory>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const char* what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
	fflush(stdout);
	if (!cond) failures++;
}

static bool near(const Vec3 &a, const Vec3 &b) { return fabsf(a.x - b.x) < 1e-3f && fabsf(a.y - b.y) < 1e-3f && fabsf(a.z - b.z) < 1e-3f; }

static Bone makeBone(int self, int parent, const std::string &name, const Vec3 &t, const Quaternion &r = Quaternion())
{
	Bone b; b.self = self; b.parent = parent; b.name = name; b.pos = t; b.rot = r; b.scale = Vec3(1, 1, 1);
	Matrix m = r.ConvertToMatrix(); m.Translate(t); b.bindPoseMat = m;
	return b;
}

int main()
{
	check(PoseTools::CounterpartName("LeftArm") == "RightArm", "LeftArm -> RightArm");
	check(PoseTools::CounterpartName("mixamorig:RightHand") == "mixamorig:LeftHand", "namespaced Mixamo names");
	check(PoseTools::CounterpartName("hand.L") == "hand.R", ".L suffix");
	check(PoseTools::CounterpartName("foot_r") == "foot_l", "_r suffix");
	check(PoseTools::CounterpartName("l_thigh") == "r_thigh", "l_ prefix");
	check(PoseTools::CounterpartName("Spine").empty(), "a centre bone has no side");

	// root -> left arm (+X) -> left hand; root -> right arm (-X) -> right hand.
	// The right side's bind is turned 180 about Y, as mirrored rigs often are,
	// so its local axes do NOT mirror the left's.
	Quaternion flip; flip.AxisToQuaternion(Vec3(0, 1, 0), 3.14159265f);
	std::vector<Bone> bones;
	bones.push_back(makeBone(0, -1, "Hips", Vec3(0, 1, 0)));
	bones.push_back(makeBone(1, 0, "LeftArm", Vec3(0.2f, 0.5f, 0)));
	bones.push_back(makeBone(2, 1, "LeftHand", Vec3(0.5f, 0, 0)));
	bones.push_back(makeBone(3, 0, "RightArm", Vec3(-0.2f, 0.5f, 0), flip));
	bones.push_back(makeBone(4, 3, "RightHand", Vec3(0.5f, 0, 0)));
	RenderingComponent rc(std::make_shared<Renderable>());
	rc.SetSkeleton(bones);
	SkeletonAnimation anim;
	SkeletonAnimationInstance* inst = anim.CreateInstance(&rc);

	check(PoseTools::CounterpartBone(inst, 1) == 3 && PoseTools::CounterpartBone(inst, 0) == 0, "counterpart ids");

	// Raise the left arm 40 degrees about Z, and shift the hips +X.
	std::vector<Matrix> locals(5);
	for (int i = 0; i < 5; i++) locals[i] = inst->GetBindPoseLocal(i);
	Quaternion up; up.AxisToQuaternion(Vec3(0, 0, 1), 0.6981317f);
	Matrix l = up.ConvertToMatrix(); l.Translate(Vec3(0.2f, 0.5f, 0)); locals[1] = l;
	Matrix h; h.Translate(Vec3(0.3f, 1.f, 0)); locals[0] = h;
	inst->ApplyPose(locals);
	const Vec3 leftHand = inst->GetBoneGlobalTransform(2).GetTranslation();
	const Vec3 rightHand = inst->GetBoneGlobalTransform(4).GetTranslation();

	const std::vector<Matrix> m = PoseTools::MirrorPose(inst, locals, 0);
	inst->ApplyPose(m);
	const Vec3 mLeft = inst->GetBoneGlobalTransform(2).GetTranslation();
	const Vec3 mRight = inst->GetBoneGlobalTransform(4).GetTranslation();
	const Vec3 mHips = inst->GetBoneGlobalTransform(0).GetTranslation();
	check(near(mRight, Vec3(-leftHand.x, leftHand.y, leftHand.z)), "the right hand lands where the left was, mirrored");
	check(near(mLeft, Vec3(-rightHand.x, rightHand.y, rightHand.z)), "the left hand takes the right's mirrored place");
	check(near(mHips, Vec3(-0.3f, 1.f, 0)), "a centre bone mirrors its own travel");

	// Mirroring twice is the original pose.
	const std::vector<Matrix> back = PoseTools::MirrorPose(inst, m, 0);
	inst->ApplyPose(back);
	check(near(inst->GetBoneGlobalTransform(2).GetTranslation(), leftHand), "mirror twice = original");

	// ---- retargeting ----------------------------------------------------
	check(PoseTools::NormalizeBoneName("mixamorig:LeftArm") == "leftarm" && PoseTools::NormalizeBoneName("Left_Arm") == "leftarm",
		"names normalise across exporters");
	{
		// Source: hips at height 1, an arm whose bind is turned 90 about Y.
		// Target: twice the size, namespaced names, arm bind turned 180 about Y.
		Quaternion q90; q90.AxisToQuaternion(Vec3(0, 1, 0), 1.5707963f);
		std::vector<Bone> sbones;
		sbones.push_back(makeBone(0, -1, "Hips", Vec3(0, 1, 0)));
		sbones.push_back(makeBone(1, 0, "Arm", Vec3(0.2f, 0.5f, 0), q90));
		sbones.push_back(makeBone(2, 1, "Hand", Vec3(0, 0, -0.5f)));
		std::vector<Bone> tbones;
		tbones.push_back(makeBone(0, -1, "mixamorig:Hips", Vec3(0, 2, 0)));
		tbones.push_back(makeBone(1, 0, "mixamorig:Arm", Vec3(0.4f, 1.f, 0), flip));
		tbones.push_back(makeBone(2, 1, "mixamorig:Hand", Vec3(1.f, 0, 0)));   // flip turns +X to -X: the same way as the source in bind
		RenderingComponent src(std::make_shared<Renderable>()); src.SetSkeleton(sbones);
		RenderingComponent dst(std::make_shared<Renderable>()); dst.SetSkeleton(tbones);
		SkeletonAnimation sa, ta;
		SkeletonAnimationInstance* si = sa.CreateInstance(&src);
		SkeletonAnimationInstance* ti = ta.CreateInstance(&dst);
		const std::map<std::string, std::string> map = PoseTools::AutoBoneMap(si, ti);
		check(map.size() == 3 && map.at("mixamorig:Arm") == "Arm", "bones map by name");

		// The clip: hips travel +1 in Z over a second, the arm turns 45 about
		// Z in model space.
		Animation clip; clip.AnimationName = "move"; clip.Duration = 1.f;
		Channel hips; hips.NodeName = "Hips";
		hips.positions.push_back(PositionData(0.f, Vec3(0, 1, 0)));
		hips.positions.push_back(PositionData(1.f, Vec3(0, 1, 1)));
		clip.Channels.push_back(hips);
		Quaternion turn; turn.AxisToQuaternion(Vec3(0, 0, 1), 0.7853982f);
		Channel arm; arm.NodeName = "Arm";
		Matrix armL = turn.ConvertToMatrix() * q90.ConvertToMatrix();   // parent (hips) is unrotated
		arm.rotations.push_back(RotationData(0.f, q90));
		arm.rotations.push_back(RotationData(1.f, armL.ConvertToQuaternion()));
		clip.Channels.push_back(arm);

		const Animation out = PoseTools::RetargetClip(si, ti, clip, map, 30.f);
		si->ApplyAnimationAtTime(clip, 1.f);
		ti->ApplyAnimationAtTime(out, 1.f);
		const Vec3 sDir = si->GetBoneGlobalTransform(2).GetTranslation() - si->GetBoneGlobalTransform(1).GetTranslation();
		const Vec3 tDir = ti->GetBoneGlobalTransform(2).GetTranslation() - ti->GetBoneGlobalTransform(1).GetTranslation();
		const Vec3 sn = sDir * (1.f / sDir.magnitude()), tn = tDir * (1.f / tDir.magnitude());
		check(near(sn, tn), "the target arm points where the source arm points");
		check(fabsf(tDir.magnitude() - 1.f) < 1e-3f, "and keeps its own length");
		check(near(ti->GetBoneGlobalTransform(0).GetTranslation(), Vec3(0, 2, 2)), "the root's travel scales with hip height");
	}

	printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
	return failures ? 1 : 0;
}
