//============================================================================
// Name        : IKSolver.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Inverse Kinematics for skeleton animation
//============================================================================

#include <Pyros3D/AnimationManager/IKSolver.h>
#include <cmath>
#include <algorithm>

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

		// A bone's model-space transform in the BIND pose, composed up its
		// parent chain - independent of whatever is posed right now.
		Matrix BindGlobal(SkeletonAnimationInstance* inst, const int32 id)
		{
			const std::vector<Bone> &bones = inst->GetSkeletonBones();
			Matrix m = inst->GetBindPoseLocal(id);
			int32 p = bones[id].parent;
			for (size_t guard = 0; p >= 0 && guard < bones.size(); guard++)
			{
				m = inst->GetBindPoseLocal(p) * m;
				p = bones[p].parent;
			}
			return m;
		}

		Vec3 BindPos(SkeletonAnimationInstance* inst, const int32 id)
		{
			return BindGlobal(inst, id).GetTranslation();
		}

		// For a straight limb: the model-space axis to fold the middle joint
		// about, read from that joint's limit. The limit's widest axis is the
		// hinge, and the side with more room is the way it folds - a knee
		// limited to 0..150 degrees about X folds the one way that range
		// allows. SolveTwoBone folds by a NEGATIVE angle about the axis it
		// is handed (straight is the widest the joint opens), so the axis
		// points opposite to the allowed side. False without a usable limit.
		bool StraightLimbHingeAxis(SkeletonAnimationInstance* inst, const int32 lower,
			const std::map<int32, JointLimit>* limits, Vec3 &outAxis)
		{
			if (!limits) return false;
			std::map<int32, JointLimit>::const_iterator it = limits->find(lower);
			if (it == limits->end() || !it->second.Enabled) return false;
			const JointLimit &lim = it->second;
			const f32 mn[3] = { lim.Min.x, lim.Min.y, lim.Min.z };
			const f32 mx[3] = { lim.Max.x, lim.Max.y, lim.Max.z };
			int k = 0;
			for (int i = 1; i < 3; i++)
				if (mx[i] - mn[i] > mx[k] - mn[k]) k = i;
			if (mx[k] - mn[k] < 1e-4f) return false;
			// The limit is measured from the bind pose in the joint's local
			// frame, so its axis in model space is the bind local's column
			// carried through the parent as it is posed now.
			const std::vector<Bone> &bones = inst->GetSkeletonBones();
			const int32 parent = bones[lower].parent;
			Matrix frame = (parent >= 0 ? inst->GetBoneGlobalTransform(parent) : Matrix())
				* inst->GetBindPoseLocal(lower);
			Vec3 axis(frame.m[k * 4 + 0], frame.m[k * 4 + 1], frame.m[k * 4 + 2]);
			if (axis.magnitudeSQR() < kEpsilon) return false;
			outAxis = (mx[k] > -mn[k]) ? axis * -1.f : axis;
			return true;
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
		//
		// Measured as a DELTA FROM THE BIND POSE, not as an absolute local
		// rotation. A limit says how far a joint may travel from rest, and a
		// bone's rest orientation is almost never the identity - so clamping
		// the absolute Euler dragged the bone's own bind orientation into
		// the limit box along with the solver's work. With min = max = 0 on
		// the two axes a hinge does not turn about, which is exactly how a
		// knee is written, that zeroed the calf's rest orientation outright:
		// the effector missed a target it was already standing on by 0.23
		// units, and a limit wide enough to constrain nothing still moved
		// the joint.
		Matrix bindRot = inst->GetBindPoseLocal(boneId);
		bindRot.Translate(Vec3(0.f, 0.f, 0.f));
		Matrix currentRot = local;
		currentRot.Translate(Vec3(0.f, 0.f, 0.f));

		Vec3 euler = (bindRot.Inverse() * currentRot).GetEulerFromRotationMatrix();
		const Vec3 wanted = euler;
		euler.x = Clamp(euler.x, lim.Min.x, lim.Max.x);
		euler.y = Clamp(euler.y, lim.Min.y, lim.Max.y);
		euler.z = Clamp(euler.z, lim.Min.z, lim.Max.z);

		// Already within the limit: leave the bone exactly as the solver
		// posed it. Writing it back unconditionally round-tripped every
		// limited joint through Euler on every solve, and
		// SetRotationFromEuler(GetEulerFromRotationMatrix(m)) does not
		// reproduce m - so a joint the limit never actually constrained
		// still drifted a little each time it was touched.
		if (std::fabs(euler.x - wanted.x) < kEpsilon
			&& std::fabs(euler.y - wanted.y) < kEpsilon
			&& std::fabs(euler.z - wanted.z) < kEpsilon) return;

		Quaternion q;
		q.SetRotationFromEuler(euler);
		Matrix clamped = bindRot * q.ConvertToMatrix();
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
		// Straightness is judged by the SINE of the angle, not by the raw
		// cross product: the same 1e-5 meant "within a hair" for a rig in
		// centimetres and "within a degree" for one in metres.
		if (bendAxis.magnitude() < 1e-3f * lenAB * lenBC)
		{
			// Chain is dead straight, so it has no plane of its own, and
			// which way it folds is exactly the knee-forward-or-backward
			// question. The old fallback was a WORLD axis - for a leg hanging
			// down -Y that put the bend about +Z and folded the knee out to
			// the side. In order of how much each source knows:
			const Vec3 limb = c - a;
			const Vec3 dir = (toTarget.magnitudeSQR() > kEpsilon) ? toTarget.normalize() : limb.normalize();
			if (pole.magnitudeSQR() > kEpsilon && (pole - a).cross(dir).magnitudeSQR() > kEpsilon)
			{
				// 1. A pole: bend towards it (the twist step below refines it).
				bendAxis = dir.cross(pole - a);
			}
			else if (StraightLimbHingeAxis(inst, lower, limits, bendAxis))
			{
				// 2. The knee's joint limit names its hinge and the side it
				// may fold to.
			}
			else
			{
				// 3. The bind pose, if the limb was modelled with any bend.
				const Vec3 ba = BindPos(inst, upper), bb = BindPos(inst, lower), bc = BindPos(inst, end);
				const Vec3 bindAxis = (ba - bb).cross(bc - bb);
				const f32 bl = (bb - ba).magnitude() * (bc - bb).magnitude();
				if (bl > kEpsilon && bindAxis.magnitude() > 1e-3f * bl)
				{
					// Carry the bind plane through whatever has turned the
					// chain since: express it in the upper bone's frame.
					Matrix bindUpper = BindGlobal(inst, upper);
					Matrix nowUpper = inst->GetBoneGlobalTransform(upper);
					bindUpper.Translate(Vec3(0.f, 0.f, 0.f));
					nowUpper.Translate(Vec3(0.f, 0.f, 0.f));
					bendAxis = nowUpper * (bindUpper.Inverse() * bindAxis);
				}
				else
				{
					// 4. Towards the target's side - in a 2D rig that is the
					// plane's normal, so a straight arm folds in the plane.
					const Vec3 side = limb.cross(toTarget);
					if (limb.magnitude() > kEpsilon && side.magnitude() > 1e-3f * limb.magnitude() * toTarget.magnitude())
						bendAxis = side;
					else
					{
						// 5. Target dead along the limb (a straight crouch).
						// The lower bone's local X: the hinge of a humanoid
						// knee and elbow in Mixamo, Blender and most DCC rigs.
						Matrix g = inst->GetBoneGlobalTransform(lower);
						bendAxis = Vec3(g.m[0], g.m[1], g.m[2]);
						if (bendAxis.cross(limb).magnitudeSQR() < kEpsilon)
							bendAxis = Vec3(g.m[8], g.m[9], g.m[10]);
					}
				}
			}
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
		const std::vector<int32> &chain, const Vec3 &target, const Vec3 &pole,
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
			// A chain lying straight along the line to its target has no bend
			// for the passes to work with: every pass keeps it on that line,
			// and the end folds back on itself instead of reaching (measured:
			// a 4-segment tail pulled straight down its length ended with its
			// tip half a segment short). Give it a small bend first - towards
			// the pole when there is one, else a fixed perpendicular, so the
			// result is still the same every time it is asked.
			bool straight = true;
			const Vec3 line = (target - root).magnitudeSQR() > kEpsilon ? (target - root).normalize()
				: (pos[n - 1] - root).normalize();
			for (size_t i = 1; i + 1 < n && straight; i++)
			{
				const Vec3 off = pos[i] - root;
				if ((off - line * off.dotProduct(line)).magnitude() > 1e-3f * total) straight = false;
			}
			if (straight && n >= 3)
			{
				Vec3 side = pole.magnitudeSQR() > kEpsilon ? (pole - root) : Vec3(0.f, 0.f, 0.f);
				side = side - line * side.dotProduct(line);
				if (side.magnitudeSQR() < kEpsilon)
				{
					side = line.cross(std::fabs(line.x) < 0.9f ? Vec3(1.f, 0.f, 0.f) : Vec3(0.f, 1.f, 0.f));
					side = side.cross(line);
				}
				side = side.normalize();
				for (size_t i = 1; i + 1 < n; i++)
				{
					// Most in the middle, nothing at the ends: an arc.
					const f32 u = (f32)i / (f32)(n - 1);
					pos[i] = pos[i] + side * (total * 0.05f * 4.f * u * (1.f - u));
				}
			}

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
		// Pole: turn each inner joint about the line through its neighbours,
		// towards the pole. Lengths are kept (the turn is about that line),
		// so the end stays where the reaching put it.
		if (pole.magnitudeSQR() > kEpsilon && n >= 3)
		{
			for (size_t i = 1; i + 1 < n; i++)
			{
				const Vec3 a = pos[i - 1], c = pos[i + 1];
				const Vec3 axis = c - a;
				if (axis.magnitudeSQR() < kEpsilon) continue;
				const Vec3 nrm = axis.normalize();
				const Vec3 j = pos[i] - a, w = pole - a;
				const Vec3 jf = j - nrm * j.dotProduct(nrm);
				const Vec3 wf = w - nrm * w.dotProduct(nrm);
				if (jf.magnitudeSQR() < kEpsilon || wf.magnitudeSQR() < kEpsilon) continue;
				const Vec3 jn = jf.normalize(), wn = wf.normalize();
				const f32 ang = atan2f(jn.cross(wn).dotProduct(nrm), jn.dotProduct(wn));
				const Quaternion q = AxisAngle(nrm, ang);
				pos[i] = a + q.ConvertToMatrix() * j;
			}
		}

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
		const uint32 iterations, const std::map<int32, JointLimit>* limits,
		const bool refreshSkinning)
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
			SolveFABRIK(inst, chain, target, pole, iterations, limits);
		if (limits && !limits->empty()) RefineWithLimits(inst, chain, target, iterations, limits);

		// RotateBoneGlobal only recomposes the hierarchy; the skinning
		// matrices are uploaded once here rather than on every joint.
		if (refreshSkinning) inst->RefreshSkinning();
		else inst->RefreshHierarchy();
		return true;
	}

	void IKSolver::RefineWithLimits(SkeletonAnimationInstance* inst,
		const std::vector<int32> &chain, const Vec3 &target,
		const uint32 iterations, const std::map<int32, JointLimit>* limits)
	{
		const size_t n = chain.size();
		if (n < 2) return;
		bool anyLimited = false;
		for (size_t i = 0; i + 1 < n; i++)
		{
			std::map<int32, JointLimit>::const_iterator it = limits->find(chain[i]);
			if (it != limits->end() && it->second.Enabled) anyLimited = true;
		}
		if (!anyLimited) return;

		// Close enough is a thousandth of the chain's length.
		f32 total = 0.f;
		for (size_t i = 0; i + 1 < n; i++) total += (BonePos(inst, chain[i + 1]) - BonePos(inst, chain[i])).magnitude();
		const f32 tol = std::max(1e-5f, total * 1e-3f);
		const uint32 passes = std::max<uint32>(iterations, 4) * 2;
		for (uint32 it = 0; it < passes; it++)
		{
			if ((BonePos(inst, chain[n - 1]) - target).magnitude() < tol) break;
			// End to root: each joint turns the end towards the target as far
			// as its limit lets it.
			for (size_t k = n - 1; k > 0; k--)
			{
				const int32 j = chain[k - 1];
				const Vec3 here = BonePos(inst, j);
				RotateBoneGlobal(inst, j, RotationBetween(BonePos(inst, chain[n - 1]) - here, target - here), here);
				ApplyLimit(inst, j, limits);
			}
		}
	}

	bool IKSolver::SolveWithEndRotation(SkeletonAnimationInstance* inst,
		const int32 rootBone, const int32 effectorBone,
		const Vec3 &target, const Vec3 &pole, const Quaternion* endRotation,
		const uint32 iterations, const std::map<int32, JointLimit>* limits)
	{
		if (!Solve(inst, rootBone, effectorBone, target, pole, iterations, limits)) return false;
		if (!endRotation) return true;

		// Model-space rotation for the effector, kept at the position the
		// solve put it: local = parent^-1 * (R at that position), with the
		// effector's own local translation (its bone length) left alone.
		const std::vector<Bone> &bones = inst->GetSkeletonBones();
		const int32 parent = bones[effectorBone].parent;
		const Matrix parentGlobal = parent >= 0 ? inst->GetBoneGlobalTransform(parent) : Matrix();
		Matrix want = endRotation->ConvertToMatrix();
		want.Translate(BonePos(inst, effectorBone));
		Matrix local = parentGlobal.Inverse() * want;
		local.Translate(inst->GetBoneLocalTransform(effectorBone).GetTranslation());
		inst->SetBoneLocalTransform(effectorBone, local);
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
