//============================================================================
// Name        : AnimatorController.h
// Description : An animation state machine: which clip a character plays,
//               and when it changes to which other one.
//
//               The file (.p3dac, JSON) holds parameters, states and
//               transitions. A state plays one clip; a transition leaves its
//               state (or any state) when its conditions on the parameters
//               hold - and, if it has one, once the clip has reached its exit
//               time - and crossfades into the next. A game sets parameters
//               ("speed" 3.2, "grounded" true, "jump" fired) and the machine
//               picks the clip; nothing in a script names a clip any more.
//
//               The controller is shared data; an Animator is one character
//               running it, against that character's SkeletonAnimationInstance.
//============================================================================

#ifndef ANIMATORCONTROLLER_H
#define ANIMATORCONTROLLER_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	class SkeletonAnimationInstance;

	enum AnimatorParamType {
		ANIMATOR_FLOAT = 0,
		ANIMATOR_BOOL = 1,
		// A bool that is consumed by the transition it fires.
		ANIMATOR_TRIGGER = 2
	};

	struct PYROS3D_API AnimatorParam {
		std::string name;
		int type = ANIMATOR_FLOAT;
		f32 defaultValue = 0.f;   // bools and triggers: 0 or 1
	};

	enum AnimatorCondOp {
		ANIMATOR_GREATER = 0,
		ANIMATOR_LESS = 1,
		ANIMATOR_EQUAL = 2,
		ANIMATOR_NOT_EQUAL = 3,
		ANIMATOR_TRUE = 4,       // bool / trigger set
		ANIMATOR_FALSE = 5
	};

	struct PYROS3D_API AnimatorCondition {
		std::string param;
		int op = ANIMATOR_GREATER;
		f32 value = 0.f;
	};

	struct PYROS3D_API AnimatorState {
		std::string name;
		// The clip, by name, in the character's own clip list.
		std::string clip;
		f32 speed = 1.f;
		bool loop = true;
		// Where the editor draws it. Not used at run time.
		Vec2 position = Vec2(0.f, 0.f);
	};

	struct PYROS3D_API AnimatorTransition {
		// "*" = any state (checked first, every frame, except into itself).
		std::string from;
		std::string to;
		// Crossfade length, seconds.
		f32 duration = 0.2f;
		// Normalised clip time (0..1, of the current loop) the transition
		// may not fire before; negative = whenever the conditions hold. With
		// no conditions at all a transition with an exit time is "when the
		// clip is done".
		f32 exitTime = -1.f;
		std::vector<AnimatorCondition> conditions;
	};

	struct PYROS3D_API AnimatorController {
		std::vector<AnimatorParam> params;
		std::vector<AnimatorState> states;
		std::vector<AnimatorTransition> transitions;
		std::string defaultState;

		int FindState(const std::string &name) const;
		int FindParam(const std::string &name) const;
		std::string UniqueStateName(const std::string &wanted) const;
		std::string UniqueParamName(const std::string &wanted) const;
	};

	PYROS3D_API std::string AnimatorControllerToString(const AnimatorController &c);
	PYROS3D_API bool AnimatorControllerFromString(const std::string &text, AnimatorController &out,
		std::string *errorOut = NULL);
	PYROS3D_API bool LoadAnimatorController(const std::string &filename, AnimatorController &out,
		std::string *errorOut = NULL);
	PYROS3D_API bool SaveAnimatorController(const std::string &filename, const AnimatorController &c,
		std::string *errorOut = NULL);

	// One character running a controller.
	class PYROS3D_API Animator {
	public:
		Animator();

		// The controller (copied) and the instance it drives. Starts the
		// default state the next Update. Parameters reset to their defaults.
		void Bind(const AnimatorController &controller, SkeletonAnimationInstance* instance);
		void Unbind();
		bool IsBound() const { return instance != NULL; }
		const AnimatorController &GetController() const { return controller; }
		SkeletonAnimationInstance* GetInstance() const { return instance; }

		// False when there is no parameter of that name (and nothing is set).
		bool SetFloat(const std::string &name, const f32 v);
		bool SetBool(const std::string &name, const bool v);
		bool SetTrigger(const std::string &name);
		bool ResetTrigger(const std::string &name);
		f32 GetFloat(const std::string &name) const;
		bool GetBool(const std::string &name) const;

		// Jump to a state now (a crossfade of `fade` seconds), whatever the
		// transitions say. False for an unknown state.
		bool Play(const std::string &state, const f32 fade = 0.f);

		// Advances the machine: checks the current state's transitions (and
		// the any-state ones) and crossfades when one fires. `dt` seconds.
		void Update(const f32 dt);

		const std::string &GetState() const { return current; }
		// Seconds in the current state, and the clip's normalised time there.
		f32 GetStateTime() const { return stateTime; }
		f32 GetStateProgress() const;
		// The transition taken most recently (-1 none), for the editor's view.
		int GetLastTransition() const { return lastTransition; }

	private:
		bool Evaluate(const AnimatorTransition &t) const;
		void Enter(const int stateIndex, const f32 fade, const int viaTransition);

		AnimatorController controller;
		SkeletonAnimationInstance* instance;
		std::map<std::string, f32> values;
		std::string current;
		int currentIndex;
		f32 stateTime;
		f32 clipDuration;
		bool started;
		int lastTransition;
	};

}

#endif
