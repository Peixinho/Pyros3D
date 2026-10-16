//============================================================================
// Name        : AnimatorComponent.h
// Description : Runs an animation state machine (AnimatorController.h) on
//               this object's skeleton. The controller is a .p3dac file the
//               scene names; parameters are what scripts set (Lua:
//               setAnimatorFloat / setAnimatorBool / setAnimatorTrigger).
//
//               It finds the skeleton instance the way IKComponent does - from
//               the object's RenderingComponent - and ticks on the main thread,
//               before the next frame's animation update reads what it chose.
//============================================================================

#ifndef ANIMATORCOMPONENT_H
#define ANIMATORCOMPONENT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/AnimationManager/AnimatorController.h>
#include <string>

namespace p3d {

	class SkeletonAnimationInstance;

	class PYROS3D_API AnimatorComponent : public IComponent {
	public:
		AnimatorComponent();
		virtual ~AnimatorComponent() {}

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() { animator.SetInstance(NULL); bound = NULL; }
		virtual void Unregister(SceneGraph* Scene) { animator.SetInstance(NULL); bound = NULL; }
		virtual uint32 GetComponentType() const { return ComponentType::Animator; }

		// Loads the controller file. False (and nothing changes) if it does not read.
		bool LoadController(const std::string &path);
		// Uses this controller (the editor's live edit), keeping the path.
		void SetController(const AnimatorController &c);
		const std::string &GetControllerPath() const { return controllerPath; }
		const AnimatorController &GetController() const { return controller; }
		bool HasController() const { return loaded; }

		Animator &GetAnimator() { return animator; }

		// Whether it drives the rig. Off until the game starts - the editor's
		// Play and the player call RenderingComponent::StartAutoPlayInScene,
		// which turns it on - so a scene being authored is not animated under
		// the pose being edited. A script making one at run time turns it on.
		void SetRunning(const bool r) { running = r; }
		bool IsRunning() const { return running; }

	private:
		SkeletonAnimationInstance* ResolveInstance();

		std::string controllerPath;
		AnimatorController controller;
		bool loaded;
		Animator animator;
		SkeletonAnimationInstance* bound;
		f64 lastTime;
		bool running;
	};

}

#endif
