//============================================================================
// Name        : UICanvas
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Root of a screen-space UI tree
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Rendering/Components/UI/UIButton.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	std::vector<IComponent*> UICanvas::Components;

	UICanvas::UICanvas(const f32 referenceWidth, const f32 referenceHeight) : IComponent()
	{
		this->referenceWidth = referenceWidth;
		this->referenceHeight = referenceHeight;
		scaleMode = UIScaleMode::MatchWidth;
		sortOrder = 0;
		pixelsPerUnit = 1.f;
		focused = NULL;
		registeredScene = NULL;
		batching = true;
		pointerWasDown = false;
	}

	UICanvas::~UICanvas() {}

	const std::vector<UIRectValue> &UICanvas::GetBatchedDrawClips() const
	{
		return batching ? batcher.GetClips() : drawClips;
	}

	const std::vector<RenderingMesh*> &UICanvas::GetBatchedDrawList()
	{
		if (!batching)
		{
			// So that turning batching back on rebuilds rather than
			// handing back whatever the last batched frame produced.
			batcher.Invalidate();
			return drawList;
		}
		return batcher.Build(drawList, drawClips);
	}

	void UICanvas::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			registeredScene = Scene;
			Registered = true;
		}
	}

	void UICanvas::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
		{
			if ((*i) == this)
			{
				Components.erase(i);
				break;
			}
		}
		registeredScene = NULL;
		Registered = false;
	}

	std::vector<IComponent*> &UICanvas::GetComponents()
	{
		return Components;
	}

	std::vector<UICanvas*> UICanvas::GetCanvasesOnScene(SceneGraph* Scene)
	{
		// Filtered from the global list against the scene each canvas was
		// registered with, rather than kept in a per-scene list on
		// SceneGraph the way lights are. There are a handful of canvases in
		// a game and hundreds of lights, so this costs nothing and keeps
		// SceneGraph out of it.
		std::vector<UICanvas*> out;
		for (size_t i = 0; i < Components.size(); i++)
		{
			UICanvas* c = static_cast<UICanvas*>(Components[i]);
			if (c->registeredScene == Scene && c->IsActive())
				out.push_back(c);
		}
		// Ascending sort order, stable so two canvases sharing an order keep
		// registration order rather than flickering between frames.
		for (size_t i = 1; i < out.size(); i++)
			for (size_t j = i; j > 0 && out[j - 1]->sortOrder > out[j]->sortOrder; j--)
			{
				UICanvas* t = out[j - 1]; out[j - 1] = out[j]; out[j] = t;
			}
		return out;
	}

	void UICanvas::SetReferenceResolution(const f32 width, const f32 height)
	{
		referenceWidth = width > 0.f ? width : 1.f;
		referenceHeight = height > 0.f ? height : 1.f;
	}

	void UICanvas::Solve(const uint32 viewportWidth, const uint32 viewportHeight)
	{
		const f32 sw = viewportWidth > 0 ? (f32)viewportWidth : 1.f;
		const f32 sh = viewportHeight > 0 ? (f32)viewportHeight : 1.f;

		f32 cw = referenceWidth, ch = referenceHeight;
		switch (scaleMode)
		{
		case UIScaleMode::ConstantPixel: cw = sw; ch = sh; break;
		case UIScaleMode::MatchWidth:    cw = referenceWidth;  ch = referenceWidth * (sh / sw); break;
		case UIScaleMode::MatchHeight:   ch = referenceHeight; cw = referenceHeight * (sw / sh); break;
		case UIScaleMode::Stretch:       cw = referenceWidth;  ch = referenceHeight; break;
		default: break;
		}

		canvasRect = UIRectValue(0.f, 0.f, cw, ch);
		pixelsPerUnit = sw / cw;

		drawList.clear();
		drawClips.clear();
		hitList.clear();
		widgetList.clear();

		if (GetOwner() == NULL) return;

		// Canvas space is the canvas GameObject's own space, so its children
		// solve against the full canvas rect with the origin at its
		// top-left corner.
		const std::vector<std::shared_ptr<GameObject> > &kids = GetOwner()->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			if (kids[i]) SolveNode(kids[i].get(), canvasRect, Vec2(0.f, 0.f), canvasRect);
	}

	// Overlap of two canvas rects, or an empty one where they do not meet -
	// a rect with no area clips everything away, which is exactly right for
	// a row scrolled fully out of its list.
	static UIRectValue Intersect(const UIRectValue &a, const UIRectValue &b)
	{
		const f32 x0 = a.x > b.x ? a.x : b.x;
		const f32 y0 = a.y > b.y ? a.y : b.y;
		const f32 x1 = (a.x + a.width) < (b.x + b.width) ? (a.x + a.width) : (b.x + b.width);
		const f32 y1 = (a.y + a.height) < (b.y + b.height) ? (a.y + a.height) : (b.y + b.height);
		return UIRectValue(x0, y0, x1 > x0 ? x1 - x0 : 0.f, y1 > y0 ? y1 - y0 : 0.f);
	}

	void UICanvas::SolveNode(GameObject* node, const UIRectValue &parentRect, const Vec2 &parentOrigin, const UIRectValue &clip)
	{
		UIRectValue childClip = clip;
		UIRectValue rect = parentRect;
		Vec2 origin = parentOrigin;
		Vec2 pivot(0.5f, 0.5f);

		const std::vector<std::shared_ptr<IComponent> > &comps = node->GetComponents();

		// Layout first, then the elements that consume it - a node without a
		// UIRect is a pass-through group, inheriting its parent's rect, which
		// is what makes a bare GameObject usable as an organisational node.
		for (size_t i = 0; i < comps.size(); i++)
		{
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::UIRect)
			{
				UIRect* r = static_cast<UIRect*>(comps[i].get());
				// Hidden takes the whole subtree with it, before anything is
				// solved: nothing to draw, nothing to hit, nothing to focus.
				if (!r->IsVisible()) return;
				r->Solve(parentRect, parentOrigin);
				rect = r->GetRect();
				origin = r->GetOriginInCanvas();
				pivot = r->GetPivot();
				// The element itself is clipped by its parent's rect; only
				// its children are clipped by its own. Nested clips
				// intersect, so a list inside a panel is bounded by both.
				if (r->IsClipChildren()) childClip = Intersect(clip, rect);
				break;
			}
		}

		hitList.push_back(std::make_pair(node, rect));
		// Any interactive component, not just buttons - a checkbox, a
		// slider and a text field all reach input the same way.
		for (size_t i = 0; i < comps.size(); i++)
		{
			if (!comps[i] || !comps[i]->IsActive()) continue;
			if (UIWidget* w = dynamic_cast<UIWidget*>(comps[i].get()))
			{ widgetList.push_back(WidgetEntry(node, rect, w)); break; }
		}

		for (size_t i = 0; i < comps.size(); i++)
		{
			if (!comps[i] || !comps[i]->IsActive()) continue;
			const uint32 type = comps[i]->GetComponentType();
			if (type == ComponentType::UIImage)
			{
				UIImage* img = static_cast<UIImage*>(comps[i].get());
				img->OnRectSolved(rect, pivot);
				std::vector<RenderingMesh*> &meshes = img->GetMeshes();
				for (size_t m = 0; m < meshes.size(); m++)
					if (meshes[m]->Active) { drawList.push_back(meshes[m]); drawClips.push_back(clip); }
			}
			else if (type == ComponentType::UIText)
			{
				UIText* txt = static_cast<UIText*>(comps[i].get());
				txt->OnRectSolved(rect, pivot);
				std::vector<RenderingMesh*> &meshes = txt->GetMeshes();
				for (size_t m = 0; m < meshes.size(); m++)
					if (meshes[m]->Active) { drawList.push_back(meshes[m]); drawClips.push_back(clip); }
			}
		}

		// Children after their own parent's element, so a later sibling
		// paints over an earlier one - the whole draw order.
		const std::vector<std::shared_ptr<GameObject> > &kids = node->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			if (kids[i]) SolveNode(kids[i].get(), rect, origin, childClip);
	}

	GameObject* UICanvas::UpdateInput(const Vec2 &canvasPoint, const bool pointerDown, const bool pointerInside)
	{
		events.clear();

		// Topmost first, so exactly one widget can be under the pointer even
		// where several overlap - the rest are told they are not, which is
		// what clears a stale hover when one moves over another.
		// A disabled widget still occludes what is under it - it is drawn
		// there, so it takes the click and does nothing with it, rather
		// than letting it fall through to whatever it is covering.
		GameObject* over = NULL;
		for (size_t i = widgetList.size(); i > 0 && !over && pointerInside; i--)
			if (widgetList[i - 1].rect.Contains(canvasPoint))
				over = widgetList[i - 1].node;

		// Pressing something focuses it, which is what makes clicking a text
		// field put the caret in it. Only on the press itself: a drag that
		// wanders over other widgets must not walk the focus along with it.
		if (pointerDown && !pointerWasDown && over != focused)
		{
			UIWidget* target = WidgetOn(over);
			if (target && target->TakesFocusOnPress()) SetFocus(over);
		}
		pointerWasDown = pointerDown;

		GameObject* clicked = NULL;
		for (size_t i = 0; i < widgetList.size(); i++)
		{
			WidgetEntry &e = widgetList[i];
			const uint32 flags = e.widget->OnPointer(e.node == over, pointerDown, canvasPoint, e.rect);
			if (flags == UIEventFlag::None) continue;
			events.push_back(WidgetEvent(e.node, e.widget, flags));
			if (flags & UIEventFlag::Clicked) clicked = e.node;
		}
		return clicked;
	}

	void UICanvas::UpdateScroll(const Vec2 &canvasPoint, const f32 delta)
	{
		events.clear();
		if (delta == 0.f) return;

		// Innermost first: a list inside a scrolling panel takes the wheel
		// while the pointer is over it, and the panel takes it otherwise.
		for (size_t i = widgetList.size(); i > 0; i--)
		{
			WidgetEntry &e = widgetList[i - 1];
			if (!e.widget->IsInteractable() || !e.rect.Contains(canvasPoint)) continue;
			const uint32 flags = e.widget->OnScroll(delta);
			if (flags == UIEventFlag::None) continue;
			events.push_back(WidgetEvent(e.node, e.widget, flags));
			return;
		}
	}

	void UICanvas::UpdateText(const std::string &utf8)
	{
		events.clear();
		UIWidget* w = WidgetOn(focused);
		if (!w || !w->IsInteractable()) return;
		const uint32 flags = w->OnText(utf8);
		if (flags != UIEventFlag::None) events.push_back(WidgetEvent(focused, w, flags));
	}

	bool UICanvas::UpdateKey(const uint32 key)
	{
		events.clear();
		UIWidget* w = WidgetOn(focused);
		if (!w || !w->IsInteractable()) return false;
		bool claimed = false;
		const uint32 flags = w->OnKey(key, claimed);
		if (flags != UIEventFlag::None) events.push_back(WidgetEvent(focused, w, flags));
		return claimed;
	}

	UIWidget* UICanvas::WidgetAt(const Vec2 &canvasPoint) const
	{
		for (size_t i = widgetList.size(); i > 0; i--)
			if (widgetList[i - 1].widget->IsInteractable() &&
				widgetList[i - 1].rect.Contains(canvasPoint))
				return widgetList[i - 1].widget;
		return NULL;
	}

	UIWidget* UICanvas::WidgetOn(GameObject* go) const
	{
		if (!go) return NULL;
		for (size_t i = 0; i < widgetList.size(); i++)
			if (widgetList[i].node == go) return widgetList[i].widget;
		return NULL;
	}

	void UICanvas::SetFocus(GameObject* go)
	{
		if (focused == go) return;
		ClearFocus();
		if (!go) return;
		UIWidget* w = WidgetOn(go);
		if (!w || !w->IsFocusable()) return;
		focused = go;
		w->SetWidgetFocused(true);
	}

	void UICanvas::ClearFocus()
	{
		if (UIWidget* w = WidgetOn(focused)) w->SetWidgetFocused(false);
		focused = NULL;
	}

	GameObject* UICanvas::FocusFirst()
	{
		for (size_t i = 0; i < widgetList.size(); i++)
		{
			if (!widgetList[i].widget->IsFocusable()) continue;
			ClearFocus();
			focused = widgetList[i].node;
			widgetList[i].widget->SetWidgetFocused(true);
			return focused;
		}
		return NULL;
	}

	GameObject* UICanvas::MoveFocus(const Vec2 &direction)
	{
		if (widgetList.empty()) return focused;
		// Nothing focused yet, or what was focused is gone: start over.
		bool stillThere = false;
		for (size_t i = 0; i < widgetList.size() && !stillThere; i++)
			if (widgetList[i].node == focused) stillThere = true;
		if (!stillThere) { focused = NULL; return FocusFirst(); }

		UIRectValue fromRect;
		for (size_t i = 0; i < widgetList.size(); i++)
			if (widgetList[i].node == focused) fromRect = widgetList[i].rect;
		const Vec2 from = fromRect.Center();

		const f32 len = sqrtf(direction.x * direction.x + direction.y * direction.y);
		if (len <= 0.0001f) return focused;
		const Vec2 dir(direction.x / len, direction.y / len);

		GameObject* best = NULL;
		UIWidget* bestWidget = NULL;
		f32 bestScore = 0.f;
		for (size_t i = 0; i < widgetList.size(); i++)
		{
			GameObject* candidate = widgetList[i].node;
			if (candidate == focused) continue;
			if (!widgetList[i].widget->IsFocusable()) continue;

			const Vec2 to = widgetList[i].rect.Center();
			const Vec2 delta(to.x - from.x, to.y - from.y);
			const f32 along = delta.x * dir.x + delta.y * dir.y;
			// Strictly in front, or "left" from a row would also match the
			// element you just came from.
			if (along <= 0.5f) continue;
			const f32 offAxis = fabsf(delta.x * dir.y - delta.y * dir.x);
			// A 45-degree cone, not just "somewhere in front". Without it,
			// pressing down at the bottom-right of a grid walks diagonally
			// to the far side rather than doing nothing - the element was
			// technically downwards, but no player reads it that way. The
			// cost is that a layout staggered by more than 45 degrees needs
			// two presses to reach; that is the better failure.
			if (offAxis > along) continue;
			// Within the cone, off-axis distance is still weighted heavily so
			// the candidate more directly in line wins among equals, which is
			// what makes a grid navigate like a grid.
			const f32 score = along + offAxis * 3.f;
			if (!best || score < bestScore) { best = candidate; bestWidget = widgetList[i].widget; bestScore = score; }
		}

		if (!best) return focused;
		ClearFocus();
		focused = best;
		bestWidget->SetWidgetFocused(true);
		return focused;
	}

	GameObject* UICanvas::ActivateFocused()
	{
		events.clear();
		UIWidget* w = WidgetOn(focused);
		if (!w) return NULL;
		const uint32 flags = w->Activate();
		if (flags == UIEventFlag::None) return NULL;
		events.push_back(WidgetEvent(focused, w, flags));
		return focused;
	}

	Vec2 UICanvas::ScreenToCanvas(const Vec2 &screenPoint) const
	{
		if (pixelsPerUnit == 0.f) return screenPoint;
		return Vec2(screenPoint.x / pixelsPerUnit, screenPoint.y / pixelsPerUnit);
	}

	GameObject* UICanvas::HitTest(const Vec2 &canvasPoint) const
	{
		// Reverse of draw order: whatever was painted last is what the user
		// sees under the cursor, so it is what the cursor hits.
		for (size_t i = hitList.size(); i > 0; i--)
			if (hitList[i - 1].second.Contains(canvasPoint))
				return hitList[i - 1].first;
		return NULL;
	}

};
