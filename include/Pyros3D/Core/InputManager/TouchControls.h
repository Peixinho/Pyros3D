//============================================================================
// Name        : TouchControls.h
// Author      : Duarte Peixinho
// Description : On-screen controls for a touch screen - sticks, buttons and a
//               look area - that turn fingers into the keys, mouse buttons
//               and mouse movement a game already reads, so a game written for
//               a keyboard and mouse plays on a phone as it is. Their layout is
//               a JSON file: the game's own, or one the player has changed by
//               dragging the controls about (edit mode) and kept.
//============================================================================

#ifndef TOUCHCONTROLS_H
#define TOUCHCONTROLS_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class SceneGraph;
	class UIRenderer;
	class UIRect;
	class UIImage;
	class UIText;
	class Texture;
	struct LoadedSceneAssets;

	class PYROS3D_API TouchControls {
	public:
		// What the controls say, told to whoever owns the input (the player: its window's own key and
		// mouse events, so nothing past it can tell a finger from a key).
		struct Emitter {
			std::function<void(uint32)> keyDown, keyUp;          // Event::Input::Keyboard
			std::function<void(uint32)> mouseDown, mouseUp;      // Event::Input::Mouse
			std::function<void(f32, f32)> look;                  // pixels moved, as a captured mouse
		};

		// One control. Where it is: x, y its centre as a fraction of the screen (0..1, y down); size its
		// diameter as a fraction of the screen's height. A look area is a rectangle: x, y its top left
		// corner, w, h its size, all fractions of the screen.
		struct Control {
			std::string id, type, label;             // type: "stick", "button", "look", "edit" (a button that opens edit mode)
			f32 x = 0.5f, y = 0.5f, size = 0.15f, w = 0.5f, h = 1.f;
			int32 up = -1, down = -1, left = -1, right = -1, run = -1;   // a stick's keys; run at full tilt
			int32 key = -1, mouse = -1;              // a button's: a key, or a mouse button
			bool toggle = false;                     // a button that stays down until pressed again
			f32 sensitivity = 1.f;                   // a look area's: pixels of mouse per pixel dragged
			bool visible = true;
		};

		static TouchControls &Instance();

		void SetEmitter(const Emitter &emitter) { this->emitter = emitter; }

		// The layout. The game's (from its own file, or the built-in one for a first-person game), and
		// on top of it the player's, where they have moved things - kept in userPath.
		bool LoadLayoutText(const std::string &jsonText);
		bool LoadLayoutFile(const std::string &path);
		void LoadDefaultLayout();
		std::string LayoutText() const;
		void SetGameLayoutFile(const std::string &path) { gameLayoutPath = path; }
		void SetUserLayoutFile(const std::string &path) { userLayoutPath = path; }
		// The player's layout if there is one, else the game's, else the default.
		void LoadPreferredLayout();
		// Back to the game's layout: the player's is forgotten (and its file removed).
		void ResetLayout();
		const std::vector<Control> &GetControls() const { return controls; }
		Control* Find(const std::string &id);

		// Shown and working at all (on by default where the screen is a touch screen).
		void SetEnabled(const bool on);
		bool IsEnabled() const { return enabled; }
		// Live: the game holds the pointer (it is being played). Not live - a menu, an inventory - the
		// controls are hidden and a finger is the pointer, a tap a click.
		void SetLive(const bool live);
		bool IsLive() const { return live; }
		// Edit mode: drag a control to move it, pinch to size it; Done keeps the layout, Reset gives back
		// the game's.
		void SetEditing(const bool on);
		bool IsEditing() const { return editing; }

		// Once a frame, after the window's events: the fingers into keys, buttons and movement.
		void Update(const uint32 width, const uint32 height);
		// Over everything else, with the UI renderer.
		void Draw(UIRenderer* ui, const uint32 width, const uint32 height, const f64 time);
		// Whether a finger is one of the controls' (not the pointer's)
		bool HoldsAnyTouch() const { return !held.empty(); }

	private:
		TouchControls();
		void ReleaseAll();
		void Build();
		void Teardown();
		void Place(const uint32 width, const uint32 height);
		bool SaveUserLayout() const;
		int32 HitControl(const Vec2 &p, const uint32 width, const uint32 height) const;
		void Press(const Control &c, const bool down);

		Emitter emitter;
		std::vector<Control> controls;
		std::string gameLayoutPath, userLayoutPath;
		bool enabled = false, live = false, editing = false, built = false;

		// Each finger held by a control: which, where it came down; and a stick's keys down now
		struct Held { int32 control; Vec2 start, last; };
		std::map<int64, Held> held;
		struct StickState { bool up = false, down = false, left = false, right = false, run = false; Vec2 knob; };
		std::map<int32, StickState> sticks;
		std::map<int32, bool> buttonDown;            // a button's (a toggle's) state

		// Edit mode: the finger moving a control, and a second one sizing it
		int64 editFinger = 0, pinchFinger = 0;
		bool editHasFinger = false, pinchHas = false;
		int32 editControl = -1;
		Vec2 editGrab;
		f32 pinchStartDist = 0.f, pinchStartSize = 0.f, pinchStartW = 0.f, pinchStartH = 0.f;

		// What is drawn
		SceneGraph* scene = NULL;
		LoadedSceneAssets* assets = NULL;
		std::shared_ptr<Texture> disc, ring;
		struct Shown { UIRect* rect = NULL; UIImage* image = NULL; UIRect* knobRect = NULL; UIImage* knob = NULL; UIRect* labelRect = NULL; UIText* text = NULL; };
		std::vector<Shown> shown;
		UIRect* doneRect = NULL; UIRect* resetRect = NULL; UIRect* hintRect = NULL;
		UIImage* doneImage = NULL; UIImage* resetImage = NULL;
		uint32 lastW = 0, lastH = 0;
	};

}

#endif /* TOUCHCONTROLS_H */
