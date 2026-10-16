// Skeletal animation and IK, without a window or a render device.
//
// A rig here is a RenderingComponent over an EMPTY Renderable with an
// authored skeleton (SetSkeleton) - exactly what a 2D character with bones
// and no sprites is, and the case that used to hang: GetMeshes() on a
// component with no meshes inserted a level per call, so UploadSkinning's
// loop over GetLODSize() never ended. An alarm turns any such hang into a
// failure instead of a stuck run.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/skeletal_animation.cpp -o /tmp/skeletal_animation \
//       -L build_ed_vk -lPyrosEngine -Wl,-rpath,$PWD/build_ed_vk
//   /tmp/skeletal_animation

#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/IKSolver.h>
#include <Pyros3D/Assets/Character2D/Character2DInstance.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>

#include <cmath>
#include <csignal>
#include <cstdio>
#include <memory>
#include <unistd.h>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const char* what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
	fflush(stdout);
	if (!cond) failures++;
}

static bool near(const Vec3 &a, const Vec3 &b, const f32 eps = 1e-3f)
{
	return fabsf(a.x - b.x) < eps && fabsf(a.y - b.y) < eps && fabsf(a.z - b.z) < eps;
}

static f32 columnLength(const Matrix &m, const int c)
{
	return sqrtf(m.m[c * 4] * m.m[c * 4] + m.m[c * 4 + 1] * m.m[c * 4 + 1] + m.m[c * 4 + 2] * m.m[c * 4 + 2]);
}

static Bone makeBone(const int self, const int parent, const std::string &name, const Vec3 &t,
	const Quaternion &r = Quaternion())
{
	Bone b;
	b.self = self;
	b.parent = parent;
	b.name = name;
	b.pos = t;
	b.rot = r;
	b.scale = Vec3(1.f, 1.f, 1.f);
	Matrix m = r.ConvertToMatrix();
	m.Translate(t);
	b.bindPoseMat = m;
	return b;
}

// A rig over no geometry at all. Kept alive by the caller.
struct Rig {
	std::shared_ptr<Renderable> renderable;
	std::unique_ptr<RenderingComponent> rc;
	SkeletonAnimation animation;
	SkeletonAnimationInstance* inst = NULL;
	explicit Rig(const std::vector<Bone> &bones)
	{
		renderable = std::make_shared<Renderable>();
		rc.reset(new RenderingComponent(renderable));
		rc->SetSkeleton(bones);
		inst = animation.CreateInstance(rc.get());
	}
	void reset()
	{
		for (uint32 b = 0; b < inst->GetNumberBones(); b++)
			inst->SetBoneLocalTransform((int32)b, inst->GetBindPoseLocal((int32)b));
		inst->RefreshSkinning();
	}
	Vec3 pos(const int32 b) const { return inst->GetBoneGlobalTransform(b).GetTranslation(); }
};

// hip(0) -> knee(1) -> ankle(2): a leg hanging straight down -Y, 0.5 + 0.5.
static std::vector<Bone> straightLeg()
{
	std::vector<Bone> bones;
	bones.push_back(makeBone(0, -1, "hip", Vec3(0.f, 1.f, 0.f)));
	bones.push_back(makeBone(1, 0, "knee", Vec3(0.f, -0.5f, 0.f)));
	bones.push_back(makeBone(2, 1, "ankle", Vec3(0.f, -0.5f, 0.f)));
	return bones;
}

static void onAlarm(int) { printf("FAIL  timed out - something loops forever\n"); fflush(stdout); _exit(1); }

int main()
{
	signal(SIGALRM, onAlarm);
	alarm(20);

	// ---- a rig with no meshes ------------------------------------------
	{
		Rig rig(straightLeg());
		rig.inst->ResetToBindPose();
		check(true, "ResetToBindPose returns on a component with no meshes");
		check(rig.rc->GetLODSize() <= 1, "GetMeshes() on no meshes does not add levels");
	}

	// ---- a 2D character with bones and no sprites (New Character 2D) -----
	{
		Character2DAsset asset;
		asset.bones.push_back(makeBone(0, -1, "Root", Vec3(0.f, 0.f, 0.f)));
		asset.bones.push_back(makeBone(1, 0, "Arm", Vec3(0.f, 1.f, 0.f)));
		RenderingComponent rc(std::make_shared<Renderable>());
		Character2DInstance built;
		ApplyCharacter2D(&rc, asset, [](const std::string &s) { return s; }, built);
		built.instance->ResetToBindPose();
		check(built.instance != NULL, "bone-only 2D character builds and poses");
	}

	// ---- sampler scale -------------------------------------------------
	{
		// A bone whose bind is turned 45 degrees, in a clip that opts into
		// scale but keys none: it must keep its bind scale of 1, not be
		// squashed by reading the diagonal of a rotated matrix.
		Quaternion q;
		q.AxisToQuaternion(Vec3(0.f, 1.f, 0.f), 3.14159265f * 0.25f);
		Matrix bind = q.ConvertToMatrix();
		bind.Translate(Vec3(0.f, 1.f, 0.f));
		Channel ch;
		ch.NodeName = "b";
		ch.positions.push_back(PositionData(0.f, Vec3(0.f, 1.f, 0.f)));
		Matrix m = SkeletonAnimationInstance::SampleChannel(ch, 0.f, true, &bind);
		check(fabsf(columnLength(m, 0) - 1.f) < 1e-4f && fabsf(columnLength(m, 2) - 1.f) < 1e-4f,
			"unkeyed scale on a rotated bind stays 1");

		// Scale keys on a rotated bone scale the bone, along its own axes.
		ch.rotations.push_back(RotationData(0.f, q));
		ch.scales.push_back(ScalingData(0.f, Vec3(2.f, 3.f, 4.f)));
		m = SkeletonAnimationInstance::SampleChannel(ch, 0.f, true, &bind);
		check(fabsf(columnLength(m, 0) - 2.f) < 1e-4f && fabsf(columnLength(m, 1) - 3.f) < 1e-4f
			&& fabsf(columnLength(m, 2) - 4.f) < 1e-4f, "scale keys apply along a rotated bone's axes");
		check(near(m.GetTranslation(), Vec3(0.f, 1.f, 0.f)), "scale does not move the bone");

		m = SkeletonAnimationInstance::SampleChannel(ch, 0.f, false, &bind);
		check(fabsf(columnLength(m, 0) - 1.f) < 1e-4f, "without the flag scale keys are ignored");
	}

	// ---- blending keeps scale ------------------------------------------
	{
		std::vector<Bone> bones;
		bones.push_back(makeBone(0, -1, "root", Vec3(0.f, 0.f, 0.f)));
		bones.push_back(makeBone(1, 0, "b", Vec3(0.f, 1.f, 0.f)));
		Rig rig(bones);
		std::vector<Animation> clips(2);
		for (int i = 0; i < 2; i++)
		{
			clips[i].AnimationName = i ? "B" : "A";
			clips[i].Duration = 1.f;
			clips[i].Flags = ANIM_FLAG_APPLY_SCALE | ANIM_FLAG_LOOP;
			Channel ch;
			ch.NodeName = "b";
			ch.positions.push_back(PositionData(0.f, Vec3(0.f, 1.f, 0.f)));
			ch.scales.push_back(ScalingData(0.f, Vec3(2.f, 2.f, 2.f)));
			ch.scales.push_back(ScalingData(1.f, Vec3(2.f, 2.f, 2.f)));
			clips[i].Channels.push_back(ch);
		}
		rig.animation.SetAnimations(clips);
		rig.inst->Play(0, 0.f, -1.f, 1.f, 0.f);
		rig.inst->Play(1, 0.f, -1.f, 1.f, 0.5f);
		rig.animation.Update(0.f);
		rig.animation.Update(0.1f);
		const Matrix &l = rig.inst->GetBoneLocalTransform(1);
		check(fabsf(columnLength(l, 0) - 2.f) < 1e-3f, "two blended scale-keyed clips keep their scale");
	}

	// ---- IK: straight limbs ---------------------------------------------
	{
		Rig rig(straightLeg());

		// Ankle raised straight up to a crouch. No pole, no limit, no bend in
		// the bind: the knee's local X is the hinge, so it folds in the YZ
		// plane - forward or back, but never out to the side.
		IKSolver::Solve(rig.inst, 0, 2, Vec3(0.f, 0.3f, 0.f), Vec3(0.f, 0.f, 0.f));
		check(near(rig.pos(2), Vec3(0.f, 0.3f, 0.f)), "straight leg reaches a crouch target");
		check(fabsf(rig.pos(1).x) < 1e-3f && fabsf(rig.pos(1).z) > 0.1f, "straight leg with no pole folds about its hinge, not sideways");

		// A pole in front puts the knee in front.
		rig.reset();
		IKSolver::Solve(rig.inst, 0, 2, Vec3(0.f, 0.3f, 0.f), Vec3(0.f, 0.5f, 2.f));
		check(rig.pos(1).z > 0.1f, "pole +Z puts the knee forward");

		// A hinge limit that only allows one side decides it, either side.
		for (int side = -1; side <= 1; side += 2)
		{
			rig.reset();
			std::map<int32, JointLimit> limits;
			limits[1] = side > 0 ? JointLimit(Vec3(0.f, 0.f, 0.f), Vec3(2.6f, 0.f, 0.f))
			                     : JointLimit(Vec3(-2.6f, 0.f, 0.f), Vec3(0.f, 0.f, 0.f));
			IKSolver::Solve(rig.inst, 0, 2, Vec3(0.f, 0.3f, 0.f), Vec3(0.f, 0.f, 0.f), 10, &limits);
			const bool reached = near(rig.pos(2), Vec3(0.f, 0.3f, 0.f), 2e-3f);
			check(reached, side > 0 ? "straight leg reaches with a +X-only knee" : "straight leg reaches with a -X-only knee");
		}

		// The same limb in a 2D rig (everything on z = 0): a straight arm
		// dragged off its line folds in the plane.
		rig.reset();
		IKSolver::Solve(rig.inst, 0, 2, Vec3(0.4f, 0.4f, 0.f), Vec3(0.f, 0.f, 0.f));
		check(near(rig.pos(2), Vec3(0.4f, 0.4f, 0.f)) && fabsf(rig.pos(1).z) < 1e-3f, "straight 2D limb folds in its plane");
	}

	// ---- IK: a bent bind is remembered -----------------------------------
	{
		// Knee modelled slightly forward: an otherwise straightened leg folds
		// that way again.
		std::vector<Bone> bones = straightLeg();
		bones[1] = makeBone(1, 0, "knee", Vec3(0.f, -0.5f, 0.05f));
		bones[2] = makeBone(2, 1, "ankle", Vec3(0.f, -0.5f, -0.05f));
		Rig rig(bones);
		// Straighten it first by aiming the whole leg out of reach.
		IKSolver::Solve(rig.inst, 0, 2, Vec3(0.f, -5.f, 0.f), Vec3(0.f, 0.f, 0.f));
		IKSolver::Solve(rig.inst, 0, 2, Vec3(0.f, 0.3f, 0.f), Vec3(0.f, 0.f, 0.f));
		check(rig.pos(1).z > 0.1f, "straightened leg folds the way its bind was bent");
	}

	// ---- IK: longer chains ------------------------------------------------
	{
		std::vector<Bone> bones;
		for (int i = 0; i < 5; i++)
			bones.push_back(makeBone(i, i - 1, "s" + std::to_string(i), i ? Vec3(0.f, 0.25f, 0.f) : Vec3(0.f, 0.f, 0.f)));
		Rig rig(bones);
		IKSolver::Solve(rig.inst, 0, 4, Vec3(0.5f, 0.6f, 0.f), Vec3(0.f, 0.f, 0.f));
		check(near(rig.pos(4), Vec3(0.5f, 0.6f, 0.f), 2e-3f), "FABRIK chain reaches its target");
	}

	// ---- IK: held end rotation, FABRIK pole ---------------------------------
	{
		Rig rig(straightLeg());
		Quaternion flat;
		flat.AxisToQuaternion(Vec3(1.f, 0.f, 0.f), 0.5f);
		IKSolver::SolveWithEndRotation(rig.inst, 0, 2, Vec3(0.f, 0.3f, 0.1f), Vec3(0.f, 0.5f, 2.f), &flat);
		const Quaternion got = rig.inst->GetBoneGlobalTransform(2).ConvertToQuaternion();
		const f32 dot = fabsf(got.x * flat.x + got.y * flat.y + got.z * flat.z + got.w * flat.w);
		check(dot > 0.9999f, "the effector holds the rotation it was given");
		check(near(rig.pos(2), Vec3(0.f, 0.3f, 0.1f)), "and still reaches the target");

		std::vector<Bone> bones;
		for (int i = 0; i < 5; i++)
			bones.push_back(makeBone(i, i - 1, "t" + std::to_string(i), i ? Vec3(0.f, 0.25f, 0.f) : Vec3(0.f, 0.f, 0.f)));
		Rig tail(bones);
		IKSolver::Solve(tail.inst, 0, 4, Vec3(0.f, 0.6f, 0.f), Vec3(1.f, 0.3f, 0.f), 30);
		check(tail.pos(2).x > 0.05f && near(tail.pos(4), Vec3(0.f, 0.6f, 0.f), 2e-3f), "a long chain bends towards its pole");
	}

	// ---- crossfade, events, root motion, tracks ------------------------------
	{
		// root(0) -> b(1). "walk" turns b to +45 about Z, "run" to -45; both
		// 1 s and looping. "walk" also travels root +Z 2 units per second
		// (root motion), fires "step" at 0.25 and 0.75, and animates a part.
		std::vector<Bone> bones;
		bones.push_back(makeBone(0, -1, "root", Vec3(0.f, 0.f, 0.f)));
		bones.push_back(makeBone(1, 0, "b", Vec3(0.f, 1.f, 0.f)));
		Rig rig(bones);
		Quaternion qa, qb;
		qa.AxisToQuaternion(Vec3(0.f, 0.f, 1.f), 0.7853982f);
		qb.AxisToQuaternion(Vec3(0.f, 0.f, 1.f), -0.7853982f);
		std::vector<Animation> clips(2);
		for (int i = 0; i < 2; i++)
		{
			Animation &c = clips[i];
			c.AnimationName = i ? "run" : "walk";
			c.Duration = 1.f;
			c.Flags = ANIM_FLAG_LOOP;
			Channel ch;
			ch.NodeName = "b";
			ch.positions.push_back(PositionData(0.f, Vec3(0.f, 1.f, 0.f)));
			ch.rotations.push_back(RotationData(0.f, i ? qb : qa));
			ch.rotations.push_back(RotationData(1.f, i ? qb : qa));
			c.Channels.push_back(ch);
		}
		Animation &walk = clips[0];
		walk.Flags |= ANIM_FLAG_ROOT_MOTION;
		Channel rootCh;
		rootCh.NodeName = "root";
		rootCh.positions.push_back(PositionData(0.f, Vec3(0.f, 0.f, 0.f)));
		rootCh.positions.push_back(PositionData(1.f, Vec3(0.f, 0.f, 2.f)));
		walk.Channels.push_back(rootCh);
		walk.Events.push_back(AnimationEvent(0.25f, "step"));
		walk.Events.push_back(AnimationEvent(0.75f, "step"));
		PropertyTrack track;
		track.Target = "mouth";
		track.Property = "z";
		PropertyKey k0; k0.Time = 0.f; k0.Value = Vec4(0.f, 0.f, 0.f, 0.f);
		PropertyKey k1; k1.Time = 1.f; k1.Value = Vec4(1.f, 0.f, 0.f, 0.f);
		track.Keys.push_back(k0); track.Keys.push_back(k1);
		walk.Tracks.push_back(track);
		PropertyTrack pic;
		pic.Target = "mouth";
		pic.Property = "sprite";
		PropertyKey p0; p0.Time = 0.f; p0.Text = "open.png";
		PropertyKey p1; p1.Time = 0.5f; p1.Text = "shut.png";
		pic.Keys.push_back(p0); pic.Keys.push_back(p1);
		walk.Tracks.push_back(pic);

		// The v2 file round-trips all of it.
		const char* path = "/tmp/pyros_skeletal_animation_test.p3da";
		check(AnimationLoader::Save(path, clips), "v2 .p3da saves");
		AnimationLoader loader;
		check(loader.Load(path) && loader.animations.size() == 2, "v2 .p3da loads");
		if (loader.animations.size() == 2)
		{
			const Animation &w = loader.animations[0];
			check(w.Events.size() == 2 && w.Events[1].Name == "step" && fabsf(w.Events[1].Time - 0.75f) < 1e-6f,
				"events round-trip");
			check(w.Tracks.size() == 2 && w.Tracks[1].Keys[1].Text == "shut.png" && w.Tracks[0].Keys[1].Value.x == 1.f,
				"property tracks round-trip");
			check(w.HasFlag(ANIM_FLAG_ROOT_MOTION), "root motion flag round-trips");
		}

		rig.animation.SetAnimations(clips);
		SkeletonAnimation &A = rig.animation;
		SkeletonAnimationInstance* I = rig.inst;
		I->Play(0, 0.f, -1.f, 1.f, 0.f);

		// 0 .. 1.6 s in 0.1 s steps: steps at 0.25, 0.75, 1.25 -> three.
		int steps = 0;
		f32 t = 0.f;
		for (int f = 0; f <= 16; f++, t += 0.1f)
		{
			A.Update(t);
			I->DeliverPending();
			std::vector<SkeletonAnimationInstance::FiredEvent> evs = I->TakeEvents();
			for (size_t e = 0; e < evs.size(); e++) if (evs[e].name == "step") steps++;
		}
		check(steps == 3, "events fire once each per loop, across the wrap");

		const Vec3 travel = I->ConsumeRootMotion();
		check(fabsf(travel.z - 3.2f) < 0.05f && fabsf(travel.x) < 1e-4f, "root motion accumulates 2 units/s across a loop");
		check(fabsf(rig.pos(0).z) < 1e-4f, "root motion is taken out of the pose");

		SkeletonAnimationInstance::TrackValue tv;
		check(I->GetTrackValue("mouth", "z", tv) && fabsf(tv.value.x - 0.6f) < 0.05f, "number tracks sample between keys");
		check(I->GetTrackValue("mouth", "sprite", tv) && tv.text == "shut.png", "text tracks step");

		// Crossfade to run over 0.5 s: half way it is between, after it only
		// run is playing and the pose is run's.
		I->CrossFade(1, 0.5f);
		A.Update(t);                       // fade starts here
		A.Update(t + 0.25f);
		Matrix bm = I->GetBoneLocalTransform(1);
		const f32 midAngle = atan2f(bm.m[1], bm.m[0]);
		A.Update(t + 0.6f);
		Matrix be = I->GetBoneLocalTransform(1);
		const f32 endAngle = atan2f(be.m[1], be.m[0]);
		check(fabsf(midAngle) < 0.1f, "crossfade half way is between the two clips");
		check(fabsf(endAngle + 0.7853982f) < 1e-3f, "crossfade ends on the new clip");
		check(I->GetNumberPlayingAnimations() == 1 && I->IsPlaying(1) && !I->IsPlaying(0),
			"crossfade stops the outgoing clip");
	}

	printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
	return failures ? 1 : 0;
}
