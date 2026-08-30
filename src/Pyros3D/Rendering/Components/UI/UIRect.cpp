//============================================================================
// Name        : UIRect
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Anchored rectangle - the layout half of a UI element
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	UIRect::UIRect() : IComponent()
	{
		// Pinned to the parent's top-left with zero size: an element that
		// has had nothing set on it yet is somewhere predictable rather
		// than stretched across the whole canvas.
		anchorMin = Vec2(0.f, 0.f);
		anchorMax = Vec2(0.f, 0.f);
		offsetMin = Vec2(0.f, 0.f);
		offsetMax = Vec2(0.f, 0.f);
		pivot = Vec2(0.5f, 0.5f);
		stateOffset = Vec2(0.f, 0.f);
		visible = true;
	}

	UIRect::~UIRect() {}

	void UIRect::SetAnchoredPosition(const Vec2 &anchor, const Vec2 &position, const Vec2 &size)
	{
		anchorMin = anchor;
		anchorMax = anchor;
		offsetMin = position;
		offsetMax = Vec2(position.x + size.x, position.y + size.y);
	}

	void UIRect::Solve(const UIRectValue &parent, const Vec2 &parentOrigin)
	{
		// Anchor rect inside the parent.
		const f32 ax0 = parent.x + anchorMin.x * parent.width;
		const f32 ax1 = parent.x + anchorMax.x * parent.width;
		const f32 ay0 = parent.y + anchorMin.y * parent.height;
		const f32 ay1 = parent.y + anchorMax.y * parent.height;

		// Offsets are insets from that rect. When an axis is pinned
		// (ax0 == ax1) this degenerates to (position, position + size),
		// which is exactly what SetAnchoredPosition() writes.
		// stateOffset shifts the whole rect without resizing it - see its
		// comment. Zero unless a UIButton is driving this element.
		const f32 x0 = ax0 + offsetMin.x + stateOffset.x;
		const f32 y0 = ay0 + offsetMin.y + stateOffset.y;
		const f32 x1 = ax1 + offsetMax.x + stateOffset.x;
		const f32 y1 = ay1 + offsetMax.y + stateOffset.y;

		rect = UIRectValue(x0, y0, x1 - x0, y1 - y0);
		origin = Vec2(rect.x + pivot.x * rect.width, rect.y + pivot.y * rect.height);

		if (GetOwner() == NULL) return;

		// Local, and y negated - the one place canvas space meets the
		// engine's y-up world. Z is left alone: draw order is the canvas's
		// hierarchy walk, not depth (the UI pass has depth testing off
		// entirely), so z stays free for whatever a user wants it for.
		const Vec3 current = GetOwner()->GetPosition();
		GetOwner()->SetPosition(Vec3(origin.x - parentOrigin.x,
		                             -(origin.y - parentOrigin.y),
		                             current.z));

		// And bake it, now. SetPosition() only flips a dirty flag; the world
		// matrix is normally rebuilt by SceneGraph::Update()'s traversal,
		// which has already run by the time a canvas solves (the canvas size
		// depends on the viewport, so the solve has to happen in the render
		// pass). Without this every element draws one frame stale - and on
		// the very first frame, at whatever transform it was constructed
		// with, which puts the entire UI in a heap at the canvas origin.
		//
		// Safe to do here because UICanvas solves strictly top-down, so this
		// object's parents are already final when it runs, and
		// UpdateTransformation() walks up to them anyway.
		GetOwner()->RefreshTransformation();
	}

};
