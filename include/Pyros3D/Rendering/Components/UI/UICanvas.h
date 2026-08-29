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

		// The element whose solved rect contains this canvas-space point,
		// topmost first (so the reverse of draw order). NULL if none.
		GameObject* HitTest(const Vec2 &canvasPoint) const;

		// Screen pixels -> canvas units, for feeding a mouse position into
		// HitTest(). Valid after Solve().
		Vec2 ScreenToCanvas(const Vec2 &screenPoint) const;

	private:

		void SolveNode(GameObject* node, const UIRectValue &parentRect, const Vec2 &parentOrigin);

		f32 referenceWidth, referenceHeight;
		uint32 scaleMode;
		int32 sortOrder;

		UIRectValue canvasRect;
		f32 pixelsPerUnit;

		std::vector<RenderingMesh*> drawList;
		// Parallel to draw order, for hit testing: every node that solved a
		// rect this frame, with the rect it solved.
		std::vector<std::pair<GameObject*, UIRectValue> > hitList;

		SceneGraph* registeredScene;

		static std::vector<IComponent*> Components;
	};

};

#endif	/* UICANVAS_H */
