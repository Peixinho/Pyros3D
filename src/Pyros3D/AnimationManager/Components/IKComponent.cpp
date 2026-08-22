//============================================================================
// Name        : IKComponent.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Runtime inverse kinematics driven by GameObject targets
//============================================================================

#include <Pyros3D/AnimationManager/Components/IKComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

namespace p3d {

	IKComponent::IKComponent()
	{
		rigLoaded = false;
		iterations = 10;
		boundInstance = NULL;
		dirty = true;
	}

	IKComponent::~IKComponent()
	{
		// The instance outlives this component when the GameObject keeps its
		// RenderingComponent, so a stale modifier would call into freed
		// memory on the next Update().
		if (boundInstance) boundInstance->RemovePoseModifier(this);
	}

	void IKComponent::Register(SceneGraph* Scene) {}
	void IKComponent::Init() {}
	void IKComponent::Destroy()
	{
		if (boundInstance)
		{
			boundInstance->RemovePoseModifier(this);
			boundInstance = NULL;
		}
	}
	void IKComponent::Unregister(SceneGraph* Scene) { Destroy(); }

	bool IKComponent::LoadRigForModel(const std::string &modelPath)
	{
		rigModelPath = modelPath;
		rigLoaded = rig.Load(RigAsset::SidecarPathFor(modelPath));
		dirty = true;
		return rigLoaded;
	}

	SkeletonAnimationInstance* IKComponent::ResolveInstance()
	{
		if (!Owner) return NULL;

		// GameObject has no typed component lookup, only the raw list.
		SkeletonAnimationInstance* inst = NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = Owner->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
		{
			const uint32 type = comps[i]->GetComponentType();
			if (type != ComponentType::RenderingComponent
				&& type != ComponentType::RenderingInstancedComponent) continue;
			RenderingComponent* rc = static_cast<RenderingComponent*>(comps[i].get());
			inst = static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation());
			if (inst) break;
		}
		if (!inst) return NULL;

		if (inst != boundInstance)
		{
			// Rebinding, e.g. the mesh was swapped or the animation was only
			// attached this frame.
			if (boundInstance) boundInstance->RemovePoseModifier(this);
			inst->AddPoseModifier(&IKComponent::SolveThunk, this);
			boundInstance = inst;
			dirty = true;
		}
		return inst;
	}

	void IKComponent::Update(const f64 time)
	{
		if (!active) return;
		// The solve itself does NOT happen here - see the class comment. All
		// this does is make sure the modifier is attached to whatever
		// instance the owner currently has.
		ResolveInstance();
	}

	void IKComponent::SolveThunk(SkeletonAnimationInstance* instance, void* userData)
	{
		static_cast<IKComponent*>(userData)->Solve(instance);
	}

	void IKComponent::Solve(SkeletonAnimationInstance* instance)
	{
		if (!active || !instance || !rigLoaded || constraints.empty()) return;

		if (dirty)
		{
			resolvedChains.clear();
			resolvedChains.resize(constraints.size());
			for (size_t i = 0; i < constraints.size(); i++)
			{
				IKChain chain;
				if (rig.ResolveChain(instance, constraints[i].ChainName, chain))
					resolvedChains[i] = chain;
				else
					resolvedChains[i].Bones.clear(); // marks it unusable
			}
			resolvedLimits = rig.ResolveLimits(instance);
			dirty = false;
		}

		// Targets are GameObjects in WORLD space; the solver works in the
		// rig's MODEL space (what GetBoneGlobalTransform returns). Converting
		// once per frame here is what keeps the solver itself free of any
		// notion of scene hierarchy.
		const Matrix worldToModel = Owner->GetWorldTransformation().Inverse();

		for (size_t i = 0; i < constraints.size(); i++)
		{
			const IKConstraint &c = constraints[i];
			if (!c.Enabled || !c.Target || c.Weight <= 0.f) continue;
			if (i >= resolvedChains.size() || resolvedChains[i].Bones.empty()) continue;

			const IKChain &chain = resolvedChains[i];
			const Vec3 target = worldToModel * c.Target->GetWorldPosition();
			const Vec3 pole = c.Pole ? (worldToModel * c.Pole->GetWorldPosition()) : chain.Pole;

			// Partial weight blends between the animated pose and the solved
			// one. Captured before solving because the solver writes in
			// place, and there is no other way back to the clip's pose.
			std::vector<Matrix> before;
			const bool blending = (c.Weight < 1.f);
			if (blending) instance->CapturePose(before);

			IKSolver::Solve(instance, chain.RootBone, chain.EffectorBone, target, pole,
				iterations, resolvedLimits.empty() ? NULL : &resolvedLimits);

			if (blending)
			{
				// Only the chain's own bones moved, so only those are blended.
				for (size_t b = 0; b < chain.Bones.size(); b++)
				{
					const int32 id = chain.Bones[b];
					const Matrix &from = before[id];
					const Matrix solved = instance->GetBoneLocalTransform(id);

					// Rotation slerped rather than lerped component-wise: a
					// matrix lerp of two rotations is not a rotation, and
					// shows up as bones visibly shrinking mid-blend.
					Matrix out = from.ConvertToQuaternion().Slerp(
						solved.ConvertToQuaternion(), c.Weight).ConvertToMatrix();
					out.Translate(from.GetTranslation().Lerp(solved.GetTranslation(), c.Weight));
					instance->SetBoneLocalTransform(id, out);
				}
				instance->RefreshHierarchy();
			}
		}
	}

}
