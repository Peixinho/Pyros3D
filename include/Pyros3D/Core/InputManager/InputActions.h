//============================================================================
// Name        : InputActions.h
// Description : What the player means, bound to what they press.
//============================================================================
#ifndef INPUTACTIONS_H
#define INPUTACTIONS_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	// A game asks for "jump", "fire", "moveX", never for Space or the A button: each action has any
	// number of bindings, and the player can change them (Capture: press the new one), with the
	// bindings saved and loaded as one string (Serialize / Deserialize).
	//
	// A binding is written as:
	//   key:W   key:Space   key:LShift   mouse:Left   pad:A   pad:RightShoulder
	//   padaxis:LeftX+   padaxis:LeftY-   padaxis:RightTrigger+
	// (a pad binding answers to any pad; "pad1:A" to the second only). An action's value is the
	// largest of its bindings' (a key or button 1 when held, an axis how far it is pushed that way,
	// past its dead zone). Down / Pressed / Released compare against a threshold (0.5).
	class PYROS3D_API InputActions {
	public:
		static void Bind(const std::string &action, const std::string &binding);
		static void Unbind(const std::string &action, const std::string &binding = std::string());   // (all, when no binding is named)
		static std::vector<std::string> GetBindings(const std::string &action);
		static std::vector<std::string> GetActions();

		static f32 Value(const std::string &action);
		static bool Down(const std::string &action);
		static bool Pressed(const std::string &action);     // went down this frame
		static bool Released(const std::string &action);    // came up this frame
		// Two actions as one axis: positive minus negative (A/D, or a stick's two halves).
		static f32 Axis(const std::string &negative, const std::string &positive);

		// Waiting for the player to press what an action should be (a remap screen): Capture()
		// returns the binding of the first key, mouse button, pad button or pad axis pushed past
		// half way since BeginCapture(), or "" while nothing has been.
		static void BeginCapture();
		static std::string Capture();

		// "jump=key:Space,pad:A;fire=mouse:Left,padaxis:RightTrigger+" - every action and its bindings.
		static std::string Serialize();
		static bool Deserialize(const std::string &text);     // (replaces every binding)

		static void SetDeadZone(const f32 dz) { deadZone = dz < 0.f ? 0.f : (dz > 0.95f ? 0.95f : dz); }

		// Once a frame, before anything reads the actions (the window's event pump calls it).
		static void NewFrame();

		// A binding string's parts (exposed for the editor's settings panel).
		struct Binding { enum Kind { Key, Mouse, PadButton, PadAxis } kind; uint32 code; int32 pad; f32 sign; };
		static bool Parse(const std::string &binding, Binding &out);
		static std::string Describe(const Binding &b);

	private:
		struct Action { std::vector<std::string> text; std::vector<Binding> parsed; f32 value = 0.f; bool down = false, was = false; };
		static std::map<std::string, Action> &Actions();
		static f32 Evaluate(const Action &a);
		static f32 deadZone;
		static bool capturing;
		static std::string captured;
		static std::vector<f32> captureBaseAxes;
	};
}

#endif /* INPUTACTIONS_H */
