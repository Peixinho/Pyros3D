//============================================================================
// Name        : BoneBind2D
// Author      : Duarte Peixinho
//============================================================================

#include <Pyros3D/Rendering/Components/BoneBind2D/BoneBind2D.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	BoneBind2D::BoneBind2D(const std::string &bone)
		: boneName(bone), offset(0.f, 0.f), scale(1.f, 1.f)
	{
	}

	BoneBind2D::~BoneBind2D() {}

	void BoneBind2D::Update(const f64 time)
	{
		if (boneName.empty()) return;
		GameObject* owner = GetOwner();
		if (!owner) return;

		// Walk up for the nearest ancestor carrying a skeleton, rather than
		// requiring the rig to be the immediate parent - a sprite is often
		// nested a layer or two down inside a rig's hierarchy.
		SkeletonAnimationInstance* inst = NULL;
		for (GameObject* p = owner->GetParent(); p != NULL; p = p->GetParent())
		{
			const std::vector<std::shared_ptr<IComponent> > &comps = p->GetComponents();
			for (size_t i = 0; i < comps.size(); i++)
			{
				if (!comps[i] || comps[i]->GetComponentType() != ComponentType::RenderingComponent) continue;
				RenderingComponent* rc = static_cast<RenderingComponent*>(comps[i].get());
				if (rc->GetSkeleton().empty()) continue;
				inst = static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation());
			}
			if (inst) break;
		}
		if (!inst) return;

		const std::vector<Bone> &bones = inst->GetSkeletonBones();
		int32 id = -1;
		for (size_t i = 0; i < bones.size(); i++)
			if (bones[i].name == boneName) { id = bones[i].self; break; }
		if (id < 0) return;

		Matrix m = inst->GetBoneGlobalTransform(id);
		if (offset.x != 0.f || offset.y != 0.f)
		{
			Matrix off;
			off.Translate(Vec3(offset.x, offset.y, 0.f));
			m = m * off;
		}
		// Scale last, so it sizes the artwork without moving the joint the
		// bone put it on.
		if (scale.x != 1.f || scale.y != 1.f)
		{
			Matrix sc;
			sc.Scale(Vec3(scale.x, scale.y, 1.f));
			m = m * sc;
		}
		owner->SetTransformationMatrix(m);
	}

};
