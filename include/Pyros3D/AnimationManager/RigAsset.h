//============================================================================
// Name        : RigAsset.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Per-skeleton authoring data - bone masks, joint limits, IK chains
//============================================================================

#ifndef RIGASSET_H
#define RIGASSET_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/AnimationManager/IKSolver.h>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	// A named set of bones, e.g. "UpperBody". Feeds
	// SkeletonAnimationInstance::CreateLayer/AddBone for layered blending.
	struct PYROS3D_API BoneMask {
		std::string Name;
		std::vector<std::string> Bones;
	};

	// An IK chain as authored - by bone NAME, so it survives the skeleton
	// being reordered and can be shared between models. IKSolver::Solve wants
	// bone ids; Resolve() below does that conversion against a live instance.
	struct PYROS3D_API IKChainDef {
		std::string Name;
		std::string RootBone;
		std::string EffectorBone;
		Vec3 Pole;
		bool UsePole;

		IKChainDef() : Pole(0.f, 0.f, 0.f), UsePole(false) {}
	};

	// Data that belongs to a SKELETON rather than to a clip or a project.
	//
	// Bone masks, joint limits and IK chains are all properties of the rig:
	// an "UpperBody" mask means the same thing for every clip played on that
	// skeleton, and a knee's limit does not change because a different
	// animation is running. Before this they had nowhere to live - masks were
	// stored in project.json keyed by ANIMATION path, which duplicated them
	// per clip and lost them entirely when the clip was renamed.
	//
	// Stored as `<model>.rig.json` beside the .p3dm. A sidecar rather than a
	// block inside the .p3dm because the .p3dm is a GENERATED artifact -
	// AssimpImporter rewrites it wholesale from the source FBX on every
	// re-import, which would destroy hand-authored joint limits that cannot
	// be re-derived from the FBX. A sidecar survives that untouched. It also
	// means the editor never has to write the model file, so no rig edit can
	// corrupt geometry.
	//
	// Everything is keyed by bone name, so two characters sharing one
	// skeleton can point at the same rig file instead of keeping two copies
	// of the same limits in sync.
	class PYROS3D_API RigAsset {
	public:

		std::vector<BoneMask> BoneMasks;
		// By bone name. Angles held in RADIANS in memory, like the rest of
		// the engine's maths - the file stores DEGREES (see Load) because a
		// joint limit is the one thing here a human actually hand-edits.
		std::map<std::string, JointLimit> JointLimits;
		std::vector<IKChainDef> IKChains;

		// `<model>.p3dm` -> `<model>.rig.json`. The fixed derivation is what
		// makes discovery automatic; nothing has to record the pairing.
		static std::string SidecarPathFor(const std::string &modelPath);

		// Missing file is NOT an error - a rig with no authored data is the
		// normal state for most models. Returns false only for a file that
		// exists but cannot be parsed.
		bool Load(const std::string &filename);
		bool Save(const std::string &filename) const;

		bool Empty() const { return BoneMasks.empty() && JointLimits.empty() && IKChains.empty(); }

		// Name-keyed limits resolved to the bone ids IKSolver::Solve wants.
		// Bones this skeleton does not have are skipped, which is the normal
		// outcome for a rig file shared across similar-but-not-identical
		// skeletons.
		std::map<int32, JointLimit> ResolveLimits(SkeletonAnimationInstance* inst) const;

		// Finds a chain by name and resolves it against `inst`. False if the
		// chain is unknown or either endpoint is not a bone here.
		bool ResolveChain(SkeletonAnimationInstance* inst, const std::string &name, IKChain &out) const;

		const IKChainDef* FindChain(const std::string &name) const;
		const BoneMask* FindMask(const std::string &name) const;
	};

}

#endif /* RIGASSET_H */
