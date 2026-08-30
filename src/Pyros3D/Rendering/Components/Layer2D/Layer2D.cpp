//============================================================================
// Name        : Layer2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A 2D scene layer - draw order and parallax for its subtree
//============================================================================

#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <cstdlib>
#include <cstdio>

namespace p3d {

	Layer2D::Layer2D(const Vec2 &parallax)
	{
		this->parallax = parallax;
		visible = true;
		basePosition = Vec3(0.f, 0.f, 0.f);
		haveBasePosition = false;
	}

	Layer2D::~Layer2D() {}

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
