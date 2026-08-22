//============================================================================
// Name        : IKSolver.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Inverse Kinematics for skeleton animation
//============================================================================

#include <Pyros3D/AnimationManager/IKSolver.h>
#include <cmath>

namespace p3d {

	namespace {

		const f32 kEpsilon = 1e-5f;

		// Quaternion::AxisToQuaternion normalises the whole quaternion at the
		// end, so a non-unit axis silently produces a different angle than
		// asked for, and a zero axis divides by zero for any angle whose
		// cosine is 0. Both are reachable here - a chain that is already
		// straight has no bend plane - so the guard belongs at every call
		// site, which means it belongs in one helper instead.
		Quaternion AxisAngle(const Vec3 &axis, const f32 angle)
		{
			const f32 len = axis.magnitude();
			if (len < kEpsilon || std::fabs(angle) < kEpsilon)
				return Quaternion(1.f, 0.f, 0.f, 0.f);

			Quaternion q;
			q.AxisToQuaternion(axis * (1.f / len), angle);
			return q;
		}

		// Shortest-arc rotation taking `from` onto `to`.
		Quaternion RotationBetween(const Vec3 &from, const Vec3 &to)
		{
			const f32 lf = from.magnitude(), lt = to.magnitude();
			if (lf < kEpsilon || lt < kEpsilon) return Quaternion(1.f, 0.f, 0.f, 0.f);

			const Vec3 a = from * (1.f / lf);
			const Vec3 b = to * (1.f / lt);
			const f32 d = a.dotProduct(b);

			// Anti-parallel: the shortest arc is a half turn about any
			// perpendicular axis, and cross(a,b) is zero here so it cannot
			// supply one. Pick whichever coordinate axis is least aligned.
			if (d < -1.f + kEpsilon)
			{
				Vec3 seed = (std::fabs(a.x) < 0.9f) ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
				return AxisAngle(a.cross(seed), 3.14159265f);
			}
			if (d > 1.f - kEpsilon) return Quaternion(1.f, 0.f, 0.f, 0.f);

			return AxisAngle(a.cross(b), acosf(d < -1.f ? -1.f : (d > 1.f ? 1.f : d)));
		}

		f32 Clamp(const f32 v, const f32 lo, const f32 hi)
		{
			return v < lo ? lo : (v > hi ? hi : v);
		}

		// Interior angle at `b` in the triangle a-b-c.
		f32 AngleAt(const Vec3 &a, const Vec3 &b, const Vec3 &c)
		{
			const Vec3 ba = a - b, bc = c - b;
			const f32 l = ba.magnitude() * bc.magnitude();
			if (l < kEpsilon) return 0.f;
			return acosf(Clamp(ba.dotProduct(bc) / l, -1.f, 1.f));
		}

		Vec3 BonePos(SkeletonAnimationInstance* inst, const int32 id)
		{
			return inst->GetBoneGlobalTransform(id).GetTranslation();
		}

	}

	std::vector<int32> IKSolver::BuildChain(SkeletonAnimationInstance* inst,
		const int32 rootBone, const int32 effectorBone)
	{
		std::vector<int32> chain;
		if (!inst) return chain;

		const uint32 boneCount = inst->GetNumberBones();
		if (rootBone < 0 || (uint32)rootBone >= boneCount) return chain;
		if (effectorBone < 0 || (uint32)effectorBone >= boneCount) return chain;

		const std::vector<Bone> &bones = inst->GetSkeletonBones();

		// Walk up from the effector. Bounded by boneCount so a malformed
		// skeleton with a parent cycle cannot spin here forever.
		int32 walk = effectorBone;
		for (uint32 guard = 0; guard <= boneCount; guard++)
		{
			chain.push_back(walk);
			if (walk == rootBone)
			{
				// Collected effector-first; the solver wants root-first.
				std::vector<int32> ordered;
				ordered.reserve(chain.size());
				for (size_t i = chain.size(); i > 0; i--) ordered.push_back(chain[i - 1]);
				return ordered;
			}
			walk = bones[walk].parent;
			if (walk < 0) break;
		}

		// Not on the same parent chain.
		chain.clear();
		return chain;
	}

	void IKSolver::RotateBoneGlobal(SkeletonAnimationInstance* inst,
		const int32 boneId, const Quaternion &rotation, const Vec3 &pivot)
	{
		const std::vector<Bone> &bones = inst->GetSkeletonBones();

		// Rotate the bone's model-space matrix about `pivot`, then express
		// the result back in the parent's frame. Writing a model-space matrix
		// straight into SetBoneLocalTransform would bake every ancestor's
		// transform into this bone and double it at playback.
		const Matrix oldGlobal = inst->GetBoneGlobalTransform(boneId);

		Matrix toPivot; toPivot.Translate(pivot * -1.f);
		Matrix fromPivot; fromPivot.Translate(pivot);
		const Matrix newGlobal = fromPivot * rotation.ConvertToMatrix() * toPivot * oldGlobal;

		const int32 parent = bones[boneId].parent;
		const Matrix parentGlobal = (parent >= 0 ? inst->GetBoneGlobalTransform(parent) : Matrix());
		inst->SetBoneLocalTransform(boneId, parentGlobal.Inverse() * newGlobal);

		// The bones below this one move with it, so their cached model-space
		// matrices are stale until the hierarchy is recomposed. Every read of
		// GetBoneGlobalTransform after this point depends on it.
		inst->RefreshHierarchy();
	}

	void IKSolver::ApplyLimit(SkeletonAnimationInstance* inst, const int32 boneId,
		const std::map<int32, JointLimit>* limits)
	{
		if (!limits) return;
		std::map<int32, JointLimit>::const_iterator it = limits->find(boneId);
		if (it == limits->end() || !it->second.Enabled) return;

		const JointLimit &lim = it->second;
		Matrix local = inst->GetBoneLocalTransform(boneId);
		const Vec3 translation = local.GetTranslation();

		// Decompose to Euler, clamp per axis, recompose. Euler is the right
		// representation here precisely because a joint limit is stated per
		// axis ("the knee bends 0..150 about X and nowhere else"), which a
		// quaternion cannot express directly.
		Vec3 euler = local.GetEulerFromRotationMatrix();
		euler.x = Clamp(euler.x, lim.Min.x, lim.Max.x);
		euler.y = Clamp(euler.y, lim.Min.y, lim.Max.y);
		euler.z = Clamp(euler.z, lim.Min.z, lim.Max.z);

		Quaternion q;
		q.SetRotationFromEuler(euler);
		Matrix clamped = q.ConvertToMatrix();
		clamped.Translate(translation);
		inst->SetBoneLocalTransform(boneId, clamped);
		inst->RefreshHierarchy();
	}

	void IKSolver::SolveTwoBone(SkeletonAnimationInstance* inst,
		const std::vector<int32> &chain, const Vec3 &target, const Vec3 &pole,
		const std::map<int32, JointLimit>* limits)
	{
		const int32 upper = chain[0], lower = chain[1], end = chain[2];

		const Vec3 a = BonePos(inst, upper);
		const Vec3 b = BonePos(inst, lower);
		const Vec3 c = BonePos(inst, end);

		const f32 lenAB = (b - a).magnitude();
		const f32 lenBC = (c - b).magnitude();
		if (lenAB < kEpsilon || lenBC < kEpsilon) return;

		// Unreachable targets are pulled in to just short of full extension
		// rather than left to produce a NaN out of acos. Stopping a hair
		// short keeps a bend axis available; a perfectly straight chain has
		// no plane to bend in and the next solve would have nothing to work
		// with.
		const f32 maxReach = (lenAB + lenBC) * 0.999f;
		const f32 minReach = std::fabs(lenAB - lenBC) * 1.001f;
		const Vec3 toTarget = target - a;
		const f32 dist = Clamp(toTarget.magnitude(), minReach > kEpsilon ? minReach : kEpsilon, maxReach);

		// STEP 1 - bend the knee to make |AC| equal `dist`.
		//
		// Only the middle joint can do this. Rotating the upper bone turns B
		// and C together as a rigid body, so it leaves the interior angle at
		// B (and therefore |AC|) completely unchanged - an earlier version
		// tried to "correct" the angle at A that way and simply never
		// reached the target.
		//
		// Axis is the current bend plane's normal, oriented so that a
		// positive rotation opens the joint: with n = (a-b) x (c-b), the
		// turn from BA to BC about n is the positive interior angle, so
		// adding to it opens further.
		Vec3 bendAxis = (a - b).cross(c - b);
		if (bendAxis.magnitudeSQR() < kEpsilon)
		{
			// Chain is dead straight, so it has no plane of its own. The
			// pole supplies one; failing that pick a perpendicular
			// deterministically - which one matters far less than picking
			// the SAME one every time (see the determinism note in the
			// header).
			const Vec3 dir = (toTarget.magnitudeSQR() > kEpsilon) ? toTarget.normalize() : Vec3(0.f, 1.f, 0.f);
			Vec3 hint = (pole.magnitudeSQR() > kEpsilon) ? (pole - a) : Vec3(0.f, 0.f, 0.f);
			if (hint.cross(dir).magnitudeSQR() < kEpsilon)
				hint = (std::fabs(dir.x) < 0.9f) ? Vec3(1.f, 0.f, 0.f) : Vec3(0.f, 1.f, 0.f);
			bendAxis = dir.cross(hint);
		}

		const f32 wantAtB = acosf(Clamp((lenAB * lenAB + lenBC * lenBC - dist * dist) / (2.f * lenAB * lenBC), -1.f, 1.f));
		const f32 haveAtB = AngleAt(a, b, c);
		RotateBoneGlobal(inst, lower, AxisAngle(bendAxis, wantAtB - haveAtB), b);
		ApplyLimit(inst, lower, limits);

		// STEP 2 - swing the whole chain so the effector lands on the target.
		// Must come second: it is defined by where the effector ended up
		// after the bend.
		const Vec3 bentC = BonePos(inst, end);
		RotateBoneGlobal(inst, upper, RotationBetween(bentC - a, target - a), a);
		ApplyLimit(inst, upper, limits);

		// STEP 3 - roll the chain about the A->target line so the knee points
		// at the pole. This is the only thing the pole controls, and it is
		// free to do last because rotating about A->target cannot move the
		// effector off the target.
		if (pole.magnitudeSQR() > kEpsilon)
		{
			const Vec3 axis = (target - a);
			if (axis.magnitudeSQR() > kEpsilon)
			{
				const Vec3 n = axis.normalize();
				const Vec3 knee = BonePos(inst, lower) - a;
				const Vec3 want = pole - a;

				// Both projected into the plane perpendicular to the swing
				// axis - the components along it are exactly what this
				// rotation cannot change.
				const Vec3 kneeFlat = knee - n * knee.dotProduct(n);
				const Vec3 wantFlat = want - n * want.dotProduct(n);
				if (kneeFlat.magnitudeSQR() > kEpsilon && wantFlat.magnitudeSQR() > kEpsilon)
				{
					const Vec3 kf = kneeFlat.normalize(), wf = wantFlat.normalize();
					const f32 angle = atan2f(kf.cross(wf).dotProduct(n), kf.dotProduct(wf));
					RotateBoneGlobal(inst, upper, AxisAngle(n, angle), a);
					ApplyLimit(inst, upper, limits);
				}
			}
		}
	}

	void IKSolver::SolveFABRIK(SkeletonAnimationInstance* inst,
		const std::vector<int32> &chain, const Vec3 &target,
		const uint32 iterations, const std::map<int32, JointLimit>* limits)
	{
		const size_t n = chain.size();

		// FABRIK solves for POSITIONS first and only then converts them back
		// into rotations, which is what makes it cheap and stable for long
		// chains - no trigonometry per joint, just repeated projection onto
		// fixed-length segments.
		std::vector<Vec3> pos(n);
		for (size_t i = 0; i < n; i++) pos[i] = BonePos(inst, chain[i]);

		std::vector<f32> len(n > 0 ? n - 1 : 0);
		f32 total = 0.f;
		for (size_t i = 0; i + 1 < n; i++) { len[i] = (pos[i + 1] - pos[i]).magnitude(); total += len[i]; }

		const Vec3 root = pos[0];

		if ((target - root).magnitude() > total)
		{
			// Out of reach: the exact answer is a straight line at the
			// target, and iterating cannot improve on it.
			const Vec3 dir = (target - root).normalize();
			for (size_t i = 0; i + 1 < n; i++) pos[i + 1] = pos[i] + dir * len[i];
		}
		else
		{
			for (uint32 it = 0; it < iterations; it++)
			{
				if ((pos[n - 1] - target).magnitude() < kEpsilon) break;

				// Backward: pin the effector to the target, walk to the root.
				pos[n - 1] = target;
				for (size_t i = n - 1; i > 0; i--)
				{
					const Vec3 d = pos[i - 1] - pos[i];
					const f32 m = d.magnitude();
					if (m < kEpsilon) continue;
					pos[i - 1] = pos[i] + d * (len[i - 1] / m);
				}

				// Forward: pin the root back where it belongs, walk out.
				pos[0] = root;
				for (size_t i = 0; i + 1 < n; i++)
				{
					const Vec3 d = pos[i + 1] - pos[i];
					const f32 m = d.magnitude();
					if (m < kEpsilon) continue;
					pos[i + 1] = pos[i] + d * (len[i] / m);
				}
			}
		}

		// Convert the solved positions back into bone rotations, root-first
		// so each bone is rotated in a hierarchy its ancestors have already
		// settled into.
		for (size_t i = 0; i + 1 < n; i++)
		{
			const Vec3 here = BonePos(inst, chain[i]);
			const Vec3 childNow = BonePos(inst, chain[i + 1]);
			RotateBoneGlobal(inst, chain[i], RotationBetween(childNow - here, pos[i + 1] - here), here);
			ApplyLimit(inst, chain[i], limits);
		}
	}

	bool IKSolver::Solve(SkeletonAnimationInstance* inst,
		const int32 rootBone, const int32 effectorBone,
		const Vec3 &target, const Vec3 &pole,
		const uint32 iterations, const std::map<int32, JointLimit>* limits)
	{
		if (!inst) return false;

		const std::vector<int32> chain = BuildChain(inst, rootBone, effectorBone);
		if (chain.size() < 2)
		{
			echo("ERROR: IKSolver::Solve - bones " + std::to_string(rootBone) + " and "
				+ std::to_string(effectorBone) + " are not on the same parent chain");
			return false;
		}

		// Exactly three bones is the knee/elbow case, and it has a closed
		// form: exact, non-iterative, and identical every time it is asked
		// the same question - which is what baking needs.
		if (chain.size() == 3)
			SolveTwoBone(inst, chain, target, pole, limits);
		else
			SolveFABRIK(inst, chain, target, iterations, limits);

		// RotateBoneGlobal only recomposes the hierarchy; the skinning
		// matrices are uploaded once here rather than on every joint.
		inst->RefreshSkinning();
		return true;
	}

	bool IKSolver::Solve(SkeletonAnimationInstance* inst, const IKChain &chain,
		const Vec3 &target, const uint32 iterations,
		const std::map<int32, JointLimit>* limits)
	{
		return Solve(inst, chain.RootBone, chain.EffectorBone, target, chain.Pole, iterations, limits);
	}

}
