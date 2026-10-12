//============================================================================
// Name        : Layer2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A 2D scene layer - draw order and parallax for its subtree
//============================================================================

#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Sprite2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteBatch2D.h>
#include <functional>
#include <cstdlib>
#include <atomic>
#include <cstdio>

namespace p3d {

	static std::atomic<int> gLiveLayer2D(0);
	uint32 Layer2D::LiveCount() { const int n = gLiveLayer2D.load(); return n > 0 ? (uint32)n : 0; }

	Layer2D::Layer2D(const Vec2 &parallax)
	{
		gLiveLayer2D++;
		this->parallax = parallax;
		visible = true;
		basePosition = Vec3(0.f, 0.f, 0.f);
		haveBasePosition = false;
		automatic = false;
		applied = true;
	}

	static std::atomic<int> gHiddenLayers(0);

	bool Layer2D::HiddenAbove(GameObject* go)
	{
		// Nearly every scene has no hidden layer, and then nobody walks
		// anything.
		if (gHiddenLayers.load() <= 0) return false;
		for (GameObject* at = go; at != NULL; at = at->GetParent())
		{
			const std::vector<std::shared_ptr<IComponent> > &comps = at->GetComponents();
			for (size_t i = 0; i < comps.size(); i++)
				if (comps[i] && comps[i]->GetComponentType() == ComponentType::Layer2D
					&& !static_cast<Layer2D*>(comps[i].get())->IsVisible())
					return true;
		}
		return false;
	}

	void Layer2D::SetVisible(const bool v)
	{
		if (v == visible) return;
		visible = v;
		gHiddenLayers += v ? -1 : 1;
		applied = false;
		ApplyVisibility();
	}

	void Layer2D::Register(SceneGraph*)
	{
		if (!applied) ApplyVisibility();
	}

	namespace {
		void ForSubtree(GameObject* go, const std::function<void(GameObject*, const std::shared_ptr<IComponent>&)> &f)
		{
			// A copy: what is called may add a component.
			const std::vector<std::shared_ptr<IComponent> > comps = go->GetComponents();
			for (size_t i = 0; i < comps.size(); i++) if (comps[i]) f(go, comps[i]);
			const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
			for (size_t i = 0; i < kids.size(); i++) if (kids[i]) ForSubtree(kids[i].get(), f);
		}
	}

	void Layer2D::ApplyVisibility()
	{
		GameObject* owner = GetOwner();
		// Not on an object yet (a scene being loaded): Register() does it.
		if (owner == NULL) return;
		applied = true;

		if (!visible)
		{
			std::vector<std::weak_ptr<IComponent> > &hidden = hiddenByLayer;
			ForSubtree(owner, [&hidden](GameObject*, const std::shared_ptr<IComponent> &c) {
				const uint32 t = c->GetComponentType();
				if ((t == ComponentType::RenderingComponent || t == ComponentType::RenderingInstancedComponent
					|| t == ComponentType::ParticleSystem) && c->IsActive())
				{
					c->Disable();
					hidden.push_back(c);
				}
			});
			return;
		}

		for (size_t i = 0; i < hiddenByLayer.size(); i++)
			if (std::shared_ptr<IComponent> c = hiddenByLayer[i].lock())
				// Still under a layer that is hidden - a layer inside a
				// layer - stays off; that one will bring it back.
				if (!HiddenAbove(c->GetOwner())) c->Enable();
		hiddenByLayer.clear();
		// Sprites and batches make their own meshes and may have made, or
		// declined to make, one while this was hidden: have them look again.
		ForSubtree(owner, [](GameObject*, const std::shared_ptr<IComponent> &c) {
			if (c->GetComponentType() == ComponentType::Sprite2D) static_cast<Sprite2D*>(c.get())->Refresh();
			else if (c->GetComponentType() == ComponentType::SpriteBatch2D) static_cast<SpriteBatch2D*>(c.get())->Refresh();
		});
	}

	void Layer2D::ScrollWithView(const Vec2 &viewCenter)
	{
		GameObject* owner = GetOwner();
		if (owner == NULL) return;
		if (!haveBasePosition)
		{
			basePosition = owner->GetPosition();
			haveBasePosition = true;
		}
		const Vec3 want(basePosition.x + viewCenter.x * (1.f - parallax.x),
			basePosition.y + viewCenter.y * (1.f - parallax.y), basePosition.z);
		const Vec3 cur = owner->GetPosition();
		// Not touched when it is already there: a layer at parallax 1 - the
		// one the whole level lives under - would otherwise be woken, and
		// everything beneath it re-transformed, every frame for nothing.
		if (cur.x == want.x && cur.y == want.y && cur.z == want.z) return;
		owner->SetPosition(want);
		owner->RefreshTransformation();
	}

	void Layer2D::ResetScroll()
	{
		GameObject* owner = GetOwner();
		if (owner != NULL && haveBasePosition)
		{
			owner->SetPosition(basePosition);
			owner->RefreshTransformation();
		}
		haveBasePosition = false;
	}

	Layer2D::~Layer2D()
	{
		gLiveLayer2D--;
		if (!visible) gHiddenLayers--;
	}

	void Layer2D::ApplyParallax(const Vec2 &scroll)
	{
		GameObject* owner = GetOwner();
		if (owner == NULL) return;

		// Latch the authored position on first use. Everything below is
		// computed from it rather than from the current position, so calling
		// this every frame does not drift and calling it twice in one frame
		// is not different from calling it once.
		if (!haveBasePosition)
		{
			basePosition = owner->GetPosition();
			haveBasePosition = true;
		}

		// A layer at parallax 1 sits still relative to the camera, so the
		// world offset it needs is scroll * (parallax - 1): at 1 that is zero
		// (it scrolls exactly with everything else), at 0 it is -scroll (it
		// cancels the camera out and stays pinned on screen).
		owner->SetPosition(Vec3(
			basePosition.x + scroll.x * (parallax.x - 1.f),
			basePosition.y + scroll.y * (parallax.y - 1.f),
			basePosition.z));
		owner->RefreshTransformation();
	}

};
