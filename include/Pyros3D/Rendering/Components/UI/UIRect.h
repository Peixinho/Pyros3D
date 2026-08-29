//============================================================================
// Name        : UIRect
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Anchored rectangle - the layout half of a UI element
//============================================================================

#ifndef UIRECT_H
#define	UIRECT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

	// A solved rectangle, in canvas units.
	//
	// Canvas space is the convention every UI tool uses and no 3D engine
	// does: origin top-left, x right, y DOWN. It is the space anchors,
	// offsets and hit-tests are all expressed in, and the only place it
	// meets the engine's y-up world is UIRect::Solve(), which negates y
	// once when it writes the owner GameObject's position. Keeping the flip
	// to that single line is deliberate - doing it in the projection instead
	// would mirror every glyph and quad, and doing it per element would put
	// a sign error in every future layout feature.
	//
	// So the canvas GameObject's local space holds canvas point (x, y) at
	// (x, -y), and UIRenderer's ortho is (0, width, -height, 0) to match.
	struct UIRectValue
	{
		UIRectValue() : x(0.f), y(0.f), width(0.f), height(0.f) {}
		UIRectValue(const f32 X, const f32 Y, const f32 W, const f32 H) : x(X), y(Y), width(W), height(H) {}

		f32 x, y, width, height;

		f32 Right() const { return x + width; }
		f32 Bottom() const { return y + height; }
		Vec2 Center() const { return Vec2(x + width * 0.5f, y + height * 0.5f); }
		bool Contains(const Vec2 &p) const
		{
			return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
		}
	};

	class PYROS3D_API UIRect : public IComponent {

	public:

		UIRect();
		virtual ~UIRect();

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::UIRect; }

		// Fractions of the parent rect, 0..1. When min and max are equal on
		// an axis the element is PINNED on it and offsets read as
		// (position, size); when they differ it STRETCHES and offsets read
		// as insets from the anchor edges. That one rule is the whole
		// reason anchors exist: it is what makes a layout survive a window
		// resize instead of only being correct at the resolution it was
		// authored at.
		void SetAnchors(const Vec2 &min, const Vec2 &max) { anchorMin = min; anchorMax = max; }
		const Vec2 &GetAnchorMin() const { return anchorMin; }
		const Vec2 &GetAnchorMax() const { return anchorMax; }

		// Insets in canvas units from the anchor rect: offsetMin is
		// (left, top), offsetMax is (right, bottom). Positive values on
		// offsetMax push the right/bottom edges further right/down, so a
		// stretched element normally wants a negative offsetMax.
		void SetOffsets(const Vec2 &min, const Vec2 &max) { offsetMin = min; offsetMax = max; }
		const Vec2 &GetOffsetMin() const { return offsetMin; }
		const Vec2 &GetOffsetMax() const { return offsetMax; }

		// Where inside its own rect the element's origin sits, 0..1 from
		// the top-left. Centre by default, which is what a quad wants.
		void SetPivot(const Vec2 &p) { pivot = p; }
		const Vec2 &GetPivot() const { return pivot; }

		// The common case, spelled without anchor arithmetic: pin to a
		// corner of the parent and give a size in canvas units.
		void SetAnchoredPosition(const Vec2 &anchor, const Vec2 &position, const Vec2 &size);

		// Filled in by UICanvas's solve, in canvas units. Valid from the
		// first canvas Update() onwards.
		const UIRectValue &GetRect() const { return rect; }

		// Solve this rect against an already-solved parent and write the
		// owner GameObject's LOCAL position - local, because a UI tree is a
		// GameObject tree, so a parent's own transform already composes into
		// its children. That is what makes rotating or scaling a panel carry
		// its contents with it, and it is why parentOrigin (the parent's
		// pivot point, in canvas units) has to come in alongside the parent
		// rect: the write is a delta from it, not an absolute.
		void Solve(const UIRectValue &parent, const Vec2 &parentOrigin);

		// A transient nudge added to the solved rect, in canvas units - what
		// a button's pressed state uses. Deliberately separate from the
		// offsets: it is applied on top of them and never serialized, so a
		// press cannot leak into the saved layout.
		void SetStateOffset(const Vec2 &o) { stateOffset = o; }
		const Vec2 &GetStateOffset() const { return stateOffset; }

		// This element's pivot point in canvas units - what its own children
		// are solved against. Valid after Solve().
		const Vec2 &GetOriginInCanvas() const { return origin; }

	private:

		Vec2 anchorMin, anchorMax;
		Vec2 offsetMin, offsetMax;
		Vec2 pivot;

		Vec2 stateOffset;

		UIRectValue rect;
		Vec2 origin;
	};

};

#endif	/* UIRECT_H */
