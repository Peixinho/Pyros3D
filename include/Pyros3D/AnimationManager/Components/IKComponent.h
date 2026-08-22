//============================================================================
// Name        : IKComponent.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Runtime inverse kinematics driven by GameObject targets
//============================================================================

#ifndef IKCOMPONENT_H
#define IKCOMPONENT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/AnimationManager/RigAsset.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <string>
#include <vector>

namespace p3d {

	class GameObject;

	// One chain pinned to a target.
	struct PYROS3D_API IKConstraint {
		// Chain name in the rig asset. Resolved lazily, since the rig can be
		// bound after the constraint is added (scene load order).
		std::string ChainName;
		// Where the effector should reach. A GameObject rather than a raw
		// position because that is what makes runtime IK worth having - the
		// target is a thing in the world (a ledge, a prop, the ground under
		// a foot), not a number known at author time.
		GameObject* Target;
		// Optional bend direction hint. Overrides the chain's authored pole.
		GameObject* Pole;
		// 0 leaves the animated pose alone, 1 fully honours the target.
		// Anything between blends the two, which is how a foot plant is faded
		// in as the foot lands rather than snapping.
		f32 Weight;
		bool Enabled;

		IKConstraint() : Target(NULL), Pole(NULL), Weight(1.f), Enabled(true) {}
	};

	// Solves IK against a skinned mesh every frame, for targets that are not
	// known when the clip is authored: feet planting on uneven ground, a hand
	// staying on a moving prop, a head tracking something.
	//
	// This is the runtime counterpart to the editor's bake. Baking handles
	// everything the animator can predict; this handles what only the running
	// game knows.
	//
	// ORDERING: the solver is registered as a
	// SkeletonAnimationInstance::PoseModifier rather than run from Update()
	// below. SkeletonAnimation::Update() rewrites the entire pose each frame,
	// so solving from a component tick would depend on this component
	// happening to update after whatever calls Update() - true or false
	// depending on object order, and silently wrong when it is false. The
	// modifier hook runs inside Update() at the only correct moment.
	class PYROS3D_API IKComponent : public IComponent {
	public:

		IKComponent();
		virtual ~IKComponent();

		virtual void Register(SceneGraph* Scene);
		virtual void Init();
		virtual void Update(const f64 time = 0);
		virtual void Destroy();
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::IK; }

		// The rig supplying chain definitions and joint limits. Without one,
		// chains cannot be resolved by name and nothing solves.
		void SetRig(const RigAsset &rig) { this->rig = rig; rigLoaded = true; dirty = true; }
		// Convenience: load `<model>.rig.json` for a model path.
		bool LoadRigForModel(const std::string &modelPath);
		const RigAsset &GetRig() const { return rig; }
		bool HasRig() const { return rigLoaded; }
		// Model path the rig was loaded from. Serialized so a scene can
		// rebuild the component without storing the rig contents twice - the
		// sidecar beside the model is the single source of truth.
		const std::string &GetRigModelPath() const { return rigModelPath; }

		void AddConstraint(const IKConstraint &c) { constraints.push_back(c); dirty = true; }
		void ClearConstraints() { constraints.clear(); dirty = true; }
		uint32 GetNumberConstraints() const { return (uint32)constraints.size(); }
		IKConstraint &GetConstraint(const uint32 i) { dirty = true; return constraints[i]; }
		const IKConstraint &GetConstraint(const uint32 i) const { return constraints[i]; }

		// Iterations handed to FABRIK for chains longer than two bones.
		void SetIterations(const uint32 n) { iterations = n; }
		uint32 GetIterations() const { return iterations; }

	private:

		// Registered with the owner's SkeletonAnimationInstance; this is what
		// actually runs, from inside SkeletonAnimation::Update().
		static void SolveThunk(SkeletonAnimationInstance* instance, void* userData);
		void Solve(SkeletonAnimationInstance* instance);

		// Finds the owner's skinned mesh instance, attaching the modifier the
		// first time it appears. The RenderingComponent may be added after
		// this one, so this is retried rather than done once in Init().
		SkeletonAnimationInstance* ResolveInstance();

		RigAsset rig;
		std::string rigModelPath;
		bool rigLoaded;
		std::vector<IKConstraint> constraints;
		uint32 iterations;

		// The instance we registered our modifier with, so it can be
		// unregistered from exactly that one.
		SkeletonAnimationInstance* boundInstance;

		// Chain resolution is cached per instance and invalidated whenever
		// the constraint list or rig changes.
		bool dirty;
		std::vector<IKChain> resolvedChains;
		std::map<int32, JointLimit> resolvedLimits;
	};

}

#endif /* IKCOMPONENT_H */
