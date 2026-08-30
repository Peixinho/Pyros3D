//============================================================================
// Name        : UICanvas
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Root of a screen-space UI tree
//============================================================================

#ifndef UICANVAS_H
#define	UICANVAS_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIWidget.h>
#include <Pyros3D/Rendering/Components/UI/UIBatcher.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	// How canvas units relate to screen pixels. Every mode below is exact -
	// there is no letterboxing option, because letterboxing is a property of
	// the viewport rather than of the canvas, and pretending otherwise puts
	// black bars into the layout model where they do not belong.
	namespace UIScaleMode
	{
		enum {
			// 1 canvas unit = 1 screen pixel. Crisp, but a layout authored
			// on one monitor is a different size on another.
			ConstantPixel = 0,
			// The canvas is exactly referenceWidth wide; its height follows
			// the real aspect ratio. The usual choice: horizontal layout is
			// stable, and a taller window just shows more vertical room.
			MatchWidth,
			// Mirror image - the canvas is exactly referenceHeight tall.
			MatchHeight,
			// Exactly reference x reference, aspect ignored. Nothing can
			// ever fall outside the canvas, at the cost of distorting when
			// the window's aspect differs from the reference's.
			Stretch
		};
	}

	class PYROS3D_API UICanvas : public IComponent {

	public:

		UICanvas(const f32 referenceWidth = 1920.f, const f32 referenceHeight = 1080.f);
		virtual ~UICanvas();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::UICanvas; }

		static std::vector<IComponent*> &GetComponents();
		static std::vector<UICanvas*> GetCanvasesOnScene(SceneGraph* Scene);

		void SetReferenceResolution(const f32 width, const f32 height);
		Vec2 GetReferenceResolution() const { return Vec2(referenceWidth, referenceHeight); }

		void SetScaleMode(const uint32 mode) { scaleMode = mode; }
		uint32 GetScaleMode() const { return scaleMode; }

		// Canvases are drawn in ascending sort order, so a pause overlay
		// sitting above a HUD is one number rather than a hierarchy edit.
		void SetSortOrder(const int32 order) { sortOrder = order; }
		int32 GetSortOrder() const { return sortOrder; }

		// Solve the whole subtree for a viewport of this pixel size, and
		// collect the draw list in hierarchy order. Called by UIRenderer,
		// not by the scene update: the canvas size depends on the viewport,
		// which the scene knows nothing about.
		void Solve(const uint32 viewportWidth, const uint32 viewportHeight);

		// Valid after Solve(). Canvas units, origin top-left.
		const UIRectValue &GetCanvasRect() const { return canvasRect; }

		// Valid after Solve(), in hierarchy order: parents before children,
		// earlier siblings before later ones. That IS the draw order - a UI
		// is a painter's-algorithm stack, not a depth-sorted scene, which is
		// why the UI pass does no sorting at all.
		const std::vector<RenderingMesh*> &GetDrawList() const { return drawList; }

		// Parallel to the draw list: the rect each entry is clipped to, in
		// canvas units. Equal to the whole canvas for anything not inside a
		// clipping element, so the renderer can scissor unconditionally
		// rather than branching per element.
		const std::vector<UIRectValue> &GetDrawClips() const { return drawClips; }
		// And the same for the batched list, which is what the UI pass
		// actually draws - a batch can only merge elements that share a
		// clip, since the scissor is per draw call.
		const std::vector<UIRectValue> &GetBatchedDrawClips() const;

		// Screen pixels per canvas unit, from the last Solve(). One under
		// ConstantPixel; whatever the reference resolution implies
		// otherwise. What converts a clip rect into a scissor.
		f32 GetPixelsPerUnit() const { return pixelsPerUnit; }

		// The same list with neighbours that share a texture or a font
		// merged into single meshes - what the UI pass actually draws. Same
		// pixels, fewer draw calls; see UIBatcher. Falls back to the raw
		// list when batching is off, which is what makes the two directly
		// comparable in a test.
		const std::vector<RenderingMesh*> &GetBatchedDrawList();

		// On by default. Off draws every element on its own, which is
		// slower but is the reference the batched path has to match.
		void SetBatching(const bool on) { batching = on; }
		bool IsBatching() const { return batching; }
		uint32 GetBatchCount() const { return batcher.GetBatchCount(); }
		uint32 GetBatchRebuildCount() const { return batcher.GetRebuildCount(); }

		// Feeds a pointer to whatever is under it. Call once a frame, after
		// Solve(), with the pointer in canvas units (ScreenToCanvas below)
		// and whether its button is held. Returns the GameObject whose
		// UIButton completed a click this frame, or NULL.
		//
		// The canvas drives this rather than each button polling input,
		// because only the canvas knows what is on top: two overlapping
		// buttons must not both light up, and the one underneath must not
		// receive the click.
		GameObject* UpdateInput(const Vec2 &canvasPoint, const bool pointerDown, const bool pointerInside = true);

		// What every widget reported during the last UpdateInput/UpdateText/
		// UpdateKey/UpdateScroll call, as (element, UIEventFlag bits). A
		// click is only one of the things a UI does: a slider dragged and a
		// field typed into both need dispatching too, and neither is a
		// return value UpdateInput could have had.
		struct WidgetEvent {
			GameObject* node;
			UIWidget* widget;
			uint32 flags;
			WidgetEvent(GameObject* n, UIWidget* w, const uint32 f) : node(n), widget(w), flags(f) {}
		};
		const std::vector<WidgetEvent> &GetEvents() const { return events; }

		// Wheel notches at a point, positive away from the user. Goes to
		// the topmost widget under the pointer that does something with it.
		void UpdateScroll(const Vec2 &canvasPoint, const f32 delta);

		// Text the platform decoded from the keyboard (UTF-8, a character
		// rather than a key) and non-printing keys (see UIKey). Both go to
		// the focused widget only. UpdateKey returns true if the widget
		// claimed the key, which is how the host knows not to also use it
		// for menu navigation.
		void UpdateText(const std::string &utf8);
		bool UpdateKey(const uint32 key);

		// The widget under a point, topmost first, or NULL. Lets a host ask
		// "is the pointer over the UI" before handing the click to the
		// world underneath.
		UIWidget* WidgetAt(const Vec2 &canvasPoint) const;

		// ---- keyboard / gamepad navigation ----
		//
		// The canvas owns focus for the same reason it owns pointer input:
		// it is the only thing that knows what elements exist and where they
		// are. Directions are in canvas space, so up is (0, -1).
		//
		// MoveFocus picks the nearest focusable element in that direction,
		// scored by how far along the direction it is plus how far off-axis -
		// which is what makes "down" from a row of buttons land on the one
		// below rather than the one that happens to be closest overall.
		// Returns the newly focused object, or the current one when there is
		// nothing that way.
		GameObject* MoveFocus(const Vec2 &direction);
		// Focus the first focusable element, for opening a menu on a pad.
		GameObject* FocusFirst();
		void ClearFocus();
		// Moves focus explicitly, ignoring anything that does not want it.
		void SetFocus(GameObject* go);
		GameObject* GetFocused() const { return focused; }
		// Presses whatever has focus. Returns it if a button was there.
		GameObject* ActivateFocused();

		// The element whose solved rect contains this canvas-space point,
		// topmost first (so the reverse of draw order). NULL if none.
		GameObject* HitTest(const Vec2 &canvasPoint) const;

		// Screen pixels -> canvas units, for feeding a mouse position into
		// HitTest(). Valid after Solve().
		Vec2 ScreenToCanvas(const Vec2 &screenPoint) const;

	private:

		void SolveNode(GameObject* node, const UIRectValue &parentRect, const Vec2 &parentOrigin, const UIRectValue &clip);

		f32 referenceWidth, referenceHeight;
		uint32 scaleMode;
		int32 sortOrder;

		UIRectValue canvasRect;
		f32 pixelsPerUnit;

		std::vector<RenderingMesh*> drawList;
		std::vector<UIRectValue> drawClips;
		UIBatcher batcher;
		bool batching;
		// Parallel to draw order, for hit testing: every node that solved a
		// rect this frame, with the rect it solved.
		std::vector<std::pair<GameObject*, UIRectValue> > hitList;

		// Every interactive element reached by the last Solve(), with the
		// rect it solved to and the widget itself, so input can be resolved
		// without walking the tree again. In draw order, which is also
		// hit-test order read backwards.
		struct WidgetEntry {
			GameObject* node;
			UIRectValue rect;
			UIWidget* widget;
			WidgetEntry(GameObject* n, const UIRectValue &r, UIWidget* w) : node(n), rect(r), widget(w) {}
		};
		std::vector<WidgetEntry> widgetList;
		std::vector<WidgetEvent> events;
		// Focus follows the press, not the drag - see UpdateInput().
		bool pointerWasDown;

		UIWidget* WidgetOn(GameObject* go) const;
		// The element of the topmost open modal, or NULL. Everything
		// outside its subtree is inert while it is up.
		GameObject* ModalRoot() const;

		// Not owning, and revalidated against widgetList every solve - an
		// element can be deleted or hidden between frames.
		GameObject* focused;

		SceneGraph* registeredScene;

		static std::vector<IComponent*> Components;
	};

};

#endif	/* UICANVAS_H */
