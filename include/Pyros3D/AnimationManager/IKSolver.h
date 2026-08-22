//============================================================================
// Name        : IKSolver.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Inverse Kinematics for skeleton animation
//============================================================================

#ifndef IKSOLVER_H
#define IKSOLVER_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <map>
#include <vector>

namespace p3d {

	// Per-bone rotation clamp, in the bone's own LOCAL frame.
	//
	// Angles are RADIANS, like everything else in the engine's maths
	// (Quaternion::SetRotationFromEuler and Matrix::GetEulerFromRotationMatrix
	// both are - DEGTORAD/RADTODEG live in Core/Math/Math.h). Passing 90
	// meaning degrees is a valid 90-radian limit and clamps nothing.
	//
	// Without limits a solver will happily bend a knee backwards: both the
	// forward and the backward solution reach the target, and nothing in the
	// maths prefers the anatomically possible one.
	struct PYROS3D_API JointLimit {
		// Euler min/max about each local axis.
		Vec3 Min;
		Vec3 Max;
		// A limit that is not enabled is not applied at all, which is not the
		// same as a limit of +-pi: clamping always costs a decompose and
		// recompose that can drift a bone that was never going to exceed it.
		bool Enabled;

		JointLimit() : Min(-3.14159265f, -3.14159265f, -3.14159265f),
			Max(3.14159265f, 3.14159265f, 3.14159265f), Enabled(false) {}
		JointLimit(const Vec3 &min, const Vec3 &max) : Min(min), Max(max), Enabled(true) {}
	};

	// A solvable chain, resolved to bone ids. Bones run root-first, effector
	// last, and are contiguous in the skeleton's parent chain.
	struct PYROS3D_API IKChain {
		std::string Name;
		int32 RootBone;
		int32 EffectorBone;
		// Filled by IKSolver::BuildChain.
		std::vector<int32> Bones;
		// Optional model-space hint for which way the joint should bend. Only
		// meaningful for chains with a single obvious bend axis (a knee, an
		// elbow); ignored when zero.
		Vec3 Pole;

		IKChain() : RootBone(-1), EffectorBone(-1), Pole(0.f, 0.f, 0.f) {}
	};

	class PYROS3D_API IKSolver {
	public:

		// Walks parents from `effectorBone` up to `rootBone` and returns the
		// chain root-first. Empty if they are not on the same parent chain,
		// which is the only way this can fail and is worth checking rather
		// than solving a chain that silently spans the wrong bones.
		static std::vector<int32> BuildChain(SkeletonAnimationInstance* inst,
			const int32 rootBone, const int32 effectorBone);

		// Poses the chain so the effector reaches `target`, both in MODEL
		// space (the space GetBoneGlobalTransform returns - multiply by the
		// owning GameObject's world matrix to go to world space).
		//
		// Dispatches to a closed-form two-bone solution when the chain is
		// exactly three bones (upper, lower, end - a leg or an arm), which is
		// exact, non-iterative and needs no `iterations`. Longer chains
		// (spine, tail, fingers) fall back to FABRIK.
		//
		// DETERMINISM: this is a pure function of the pose it is handed and
		// the target. It never remembers anything between calls. That matters
		// specifically for baking - the editor solves per frame across a
		// range, and if the solver were seeded from the previous frame's
		// result then scrubbing backwards would bake different keys than
		// scrubbing forwards. Callers must therefore establish the pose
		// first (ApplyAnimationAtTime) and then solve, every time.
		//
		// Writes local transforms through SetBoneLocalTransform and leaves the
		// skinning matrices refreshed. Returns false if the chain could not be
		// built.
		static bool Solve(SkeletonAnimationInstance* inst,
			const int32 rootBone, const int32 effectorBone,
			const Vec3 &target, const Vec3 &pole,
			const uint32 iterations = 10,
			const std::map<int32, JointLimit>* limits = NULL);

		// Same, for a chain whose bones are already resolved.
		static bool Solve(SkeletonAnimationInstance* inst, const IKChain &chain,
			const Vec3 &target, const uint32 iterations = 10,
			const std::map<int32, JointLimit>* limits = NULL);

	private:

		// Exact law-of-cosines solution for [upper, lower, end].
		static void SolveTwoBone(SkeletonAnimationInstance* inst,
			const std::vector<int32> &chain, const Vec3 &target, const Vec3 &pole,
			const std::map<int32, JointLimit>* limits);

		// Iterative backward/forward reaching for longer chains.
		static void SolveFABRIK(SkeletonAnimationInstance* inst,
			const std::vector<int32> &chain, const Vec3 &target,
			const uint32 iterations, const std::map<int32, JointLimit>* limits);

		// Rotates a bone about `pivot` (model space) by `rotation`, writing
		// the result back as a local transform.
		static void RotateBoneGlobal(SkeletonAnimationInstance* inst,
			const int32 boneId, const Quaternion &rotation, const Vec3 &pivot);

		// Clamps one bone's local rotation into its limit, if it has one.
		static void ApplyLimit(SkeletonAnimationInstance* inst, const int32 boneId,
			const std::map<int32, JointLimit>* limits);
	};

}

#endif /* IKSOLVER_H */
