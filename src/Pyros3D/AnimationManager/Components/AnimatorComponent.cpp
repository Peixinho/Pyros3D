//============================================================================
// Name        : AnimatorComponent.cpp
// Description : See AnimatorComponent.h.
//============================================================================

#include <Pyros3D/AnimationManager/Components/AnimatorComponent.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Core/Logs/Log.h>

namespace p3d {

	AnimatorComponent::AnimatorComponent() : loaded(false), bound(NULL), lastTime(-1.0), running(false) {}

	bool AnimatorComponent::LoadController(const std::string &path)
	{
		AnimatorController c;
		std::string err;
		if (!LoadAnimatorController(path, c, &err))
		{
			echo("WARNING: Animator - " + err);
			return false;
		}
		controllerPath = path;
		SetController(c);
		return true;
	}

	void AnimatorComponent::SetController(const AnimatorController &c)
	{
		controller = c;
		loaded = true;
		// The parameters exist now, so a script can set them before the first
		// update attaches the skeleton (and starts the default states).
		bound = NULL;
		animator.Bind(controller, NULL);
	}

	SkeletonAnimationInstance* AnimatorComponent::ResolveInstance()
	{
		if (!Owner) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = Owner->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
		{
			const uint32 type = comps[i]->GetComponentType();
			if (type != ComponentType::RenderingComponent && type != ComponentType::RenderingInstancedComponent) continue;
			RenderingComponent* rc = static_cast<RenderingComponent*>(comps[i].get());
			if (SkeletonAnimationInstance* inst = static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation()))
				return inst;
		}
		return NULL;
	}

	void AnimatorComponent::Update(const f64 time)
	{
		if (!active || !loaded || !running) { lastTime = time; return; }
		SkeletonAnimationInstance* inst = ResolveInstance();
		if (inst != bound)
		{
			bound = inst;
			animator.SetInstance(inst);
		}
		// `time` is the scene clock; the machine wants the step.
		f32 dt = lastTime < 0.0 ? 0.f : (f32)(time - lastTime);
		if (dt < 0.f || dt > 0.5f) dt = 0.f;   // a reset clock or a stall is not a half-second of animation
		lastTime = time;
		animator.Update(dt);
	}

}
