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
		registeredScene = NULL;
	}

	UICanvas::~UICanvas() {}

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
		hitList.clear();

		if (GetOwner() == NULL) return;

		// Canvas space is the canvas GameObject's own space, so its children
		// solve against the full canvas rect with the origin at its
		// top-left corner.
		const std::vector<std::shared_ptr<GameObject> > &kids = GetOwner()->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			if (kids[i]) SolveNode(kids[i].get(), canvasRect, Vec2(0.f, 0.f));
	}

	void UICanvas::SolveNode(GameObject* node, const UIRectValue &parentRect, const Vec2 &parentOrigin)
	{
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
				r->Solve(parentRect, parentOrigin);
				rect = r->GetRect();
				origin = r->GetOriginInCanvas();
				pivot = r->GetPivot();
				break;
			}
		}

		hitList.push_back(std::make_pair(node, rect));

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
					if (meshes[m]->Active) drawList.push_back(meshes[m]);
			}
			else if (type == ComponentType::UIText)
			{
				UIText* txt = static_cast<UIText*>(comps[i].get());
				txt->OnRectSolved(rect, pivot);
				std::vector<RenderingMesh*> &meshes = txt->GetMeshes();
				for (size_t m = 0; m < meshes.size(); m++)
					if (meshes[m]->Active) drawList.push_back(meshes[m]);
			}
		}

		// Children after their own parent's element, so a later sibling
		// paints over an earlier one - the whole draw order.
		const std::vector<std::shared_ptr<GameObject> > &kids = node->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			if (kids[i]) SolveNode(kids[i].get(), rect, origin);
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
