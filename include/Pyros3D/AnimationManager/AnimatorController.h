//============================================================================
// Name        : AnimatorController.h
// Description : An animation state machine: which clip a character plays,
//               and when it changes to which other one.
//
//               The file (.p3dac, JSON) holds parameters, states and
//               transitions. A state plays one clip - or a BLEND of several
//               spread along a parameter (walk at speed 1, run at 4: one
//               "Locomotion" state that is always the right mix, feet in step)
//               - and a transition leaves its state (or any state) when its
//               conditions on the parameters hold, and, if it has one, once the
//               clip has reached its exit time; it crossfades into the next.
//
//               LAYERS are further machines on part of the skeleton - the
//               bones under one root bone - over the base: the legs walk while
//               the upper body aims, reloads or throws, each with its own
//               states and transitions, all reading the same parameters.
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

	// One clip of a blend state, and where on the blend parameter it is
	// fully itself.
	struct PYROS3D_API AnimatorBlendClip {
		std::string clip;
		f32 at = 0.f;
	};

	struct PYROS3D_API AnimatorState {
		std::string name;
		// The clip, by name, in the character's own clip list. Ignored for a
		// blend state. Empty on a layer state = the layer shows nothing (the
		// base underneath plays through).
		std::string clip;
		f32 speed = 1.f;
		bool loop = true;
		// A 1D blend: the clips below, mixed by `blendParam`. Between two
		// neighbours the weights go linearly from one to the other; past
		// the ends the nearest one plays alone. The clips are kept in step -
		// each runs at the speed that gives them all the blended length - so
		// a walk and a run of different lengths do not drift apart.
		std::string blendParam;
		std::vector<AnimatorBlendClip> blend;
		// Pace by the parameter: the clips' `at` are the speeds over the
		// ground their feet cover (the parameter is that speed), and the mix
		// plays at parameter / (its blended `at`) - a walk made for 1.6 m/s
		// played at 1 m/s goes at 0.62x instead of sliding - within
		// paceMin..paceMax. Off: the mix plays at `speed`.
		bool pace = false;
		f32 paceMin = 0.6f, paceMax = 1.35f;
		bool IsBlend() const { return !blend.empty(); }
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

	// One machine: states, the transitions between them, where it starts.
	// The controller itself is the base layer (the whole skeleton); its
	// `layers` are machines on the bones under `rootBone`.
	struct PYROS3D_API AnimatorLayer {
		std::string name;       // layers only - the base is unnamed
		std::string rootBone;   // layers only: this bone and everything under it
		std::vector<AnimatorState> states;
		std::vector<AnimatorTransition> transitions;
		std::string defaultState;

		int FindState(const std::string &name) const;
		std::string UniqueStateName(const std::string &wanted) const;
	};

	struct PYROS3D_API AnimatorController : public AnimatorLayer {
		std::vector<AnimatorParam> params;
		std::vector<AnimatorLayer> layers;

		int FindParam(const std::string &name) const;
		int FindLayer(const std::string &name) const;
		std::string UniqueParamName(const std::string &wanted) const;
		std::string UniqueLayerName(const std::string &wanted) const;
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

		// The controller (copied) and the instance it drives. Starts every
		// layer's default state the next Update. Parameters reset to defaults.
		// The instance may be NULL: the parameters exist (and can be set) before
		// there is a skeleton to drive.
		void Bind(const AnimatorController &controller, SkeletonAnimationInstance* instance);
		// The same controller on another (or the first) instance, keeping the
		// parameter values and any state Play()ed meanwhile - a script's init
		// runs before the character's first update attaches its skeleton.
		void SetInstance(SkeletonAnimationInstance* instance);
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
		// transitions say. `layer` "" = the base. False for an unknown one.
		bool Play(const std::string &state, const f32 fade = 0.f, const std::string &layer = std::string());

		// Advances every layer: checks the current state's transitions (and
		// the any-state ones), crossfades when one fires, and re-weighs blend
		// states. `dt` seconds.
		void Update(const f32 dt);

		// The base layer's state, or a named layer's.
		const std::string &GetState(const std::string &layer = std::string()) const;
		// Seconds in the base layer's current state, and its normalised time.
		f32 GetStateTime() const { return machines.empty() ? 0.f : machines[0].stateTime; }
		f32 GetStateProgress(const std::string &layer = std::string()) const;
		// The transition the base layer (or a named one) took last (-1 none).
		int GetLastTransition(const std::string &layer = std::string()) const;
		// Current weights of a blend state's clips, base layer (for the editor).
		std::vector<f32> GetBlendWeights(const std::string &layer = std::string()) const;

	private:
		struct Machine {
			const AnimatorLayer* def = NULL;   // into `controller`
			std::string layerName;             // "" for the base
			std::string current;
			int currentIndex = -1;
			f32 stateTime = 0.f;
			f32 clipDuration = 0.f;            // blended, for a blend state
			bool started = false;
			int lastTransition = -1;
			std::vector<f32> weights;
		};
		bool Evaluate(const AnimatorTransition &t) const;
		void Enter(Machine &m, const int stateIndex, const f32 fade, const int viaTransition);
		void Reweigh(Machine &m, const f32 fadeIn);
		f32 Progress(const Machine &m) const;
		const Machine* FindMachine(const std::string &layer) const;
		void UpdateMachine(Machine &m, const f32 dt);

		AnimatorController controller;
		SkeletonAnimationInstance* instance;
		std::map<std::string, f32> values;
		std::vector<Machine> machines;
	};

}

#endif
